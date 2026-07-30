#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "as5600.h"
#include "foc_pwm.h"
#include "motor_control.h"
#include "ti_msp_dl_config.h"
#include "uart_command.h"

static volatile uint32_t g_controlTicks;

static uint32_t take_control_ticks(void)
{
    uint32_t primask = __get_PRIMASK();
    uint32_t ticks;

    __disable_irq();
    ticks = g_controlTicks;
    g_controlTicks = 0U;
    if (primask == 0U) {
        __enable_irq();
    }
    return ticks;
}

int main(void)
{
    bool sensorReady;

    SYSCFG_DL_init();

    /* Hardware is kept disabled until CAL followed by EN 1. */
    foc_pwm_init();
    sensorReady = as5600_init();
    motor_control_init();
    uart_command_init();

    NVIC_ClearPendingIRQ(CMD_UART_INST_INT_IRQN);
    NVIC_EnableIRQ(CMD_UART_INST_INT_IRQN);
    NVIC_ClearPendingIRQ(CONTROL_TICK_INST_INT_IRQN);
    NVIC_EnableIRQ(CONTROL_TICK_INST_INT_IRQN);
    DL_TimerG_startCounter(CONTROL_TICK_INST);
    __enable_irq();

    uart_command_send_string(
        "\r\nBall-and-beam motor controller ready. Motor is DISABLED.\r\n");
    uart_command_send_string(sensorReady ?
        "AS5600 ready. Send STATUS, then CAL, ZERO, EN 1.\r\n" :
        "AS5600 not ready. Check wiring/magnet, then STATUS and CLEAR.\r\n");

    while (1) {
        uint32_t ticks;

        uart_command_process();
        ticks = take_control_ticks();
        if (ticks > 0U) {
            const MotorControlState *state = motor_control_get_state();
            bool enableGrace = motor_control_take_enable_grace();
            float dt;

            if ((ticks > CONTROL_OVERRUN_LIMIT) && state->enabled &&
                !enableGrace) {
                motor_control_force_fault(MOTOR_FAULT_CONTROL_OVERRUN);
            } else {
                /*
                 * CAL is intentionally blocking while the motor is disabled.
                 * If EN 1 was already queued behind CAL, discard that old
                 * disabled-time backlog on the first enabled update.
                 */
                if (ticks > CONTROL_OVERRUN_LIMIT) {
                    ticks = 1U;
                }
                dt = (float) ticks / CONTROL_FREQUENCY_HZ;
                (void) as5600_update(dt);
                motor_control_update(dt);
            }
        }
        __WFI();
    }
}

void CONTROL_TICK_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(CONTROL_TICK_INST)) {
    case DL_TIMER_IIDX_ZERO:
        if (g_controlTicks < UINT32_MAX) {
            g_controlTicks++;
        }
        break;
    default:
        break;
    }
}

void CMD_UART_INST_IRQHandler(void)
{
    uart_command_handle_rx_interrupt();
}
