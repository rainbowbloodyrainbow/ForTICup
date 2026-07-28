#include "ti_msp_dl_config.h"
#include "output.h"

#define BREATH_CYCLE_MS       (4000U)
#define BREATH_HALF_CYCLE_MS  (2000U)
#define BREATH_RAMP_MS        (1000U)

/* 系统运行的毫秒数 */
volatile uint32_t g_msTicks = 0;

/* SysTick 每 1 ms 进入一次 */
void SysTick_Handler(void)
{
    g_msTicks++;
}

int main(void)
{
    uint32_t lastPwmUpdateTime = 0;

    SYSCFG_DL_init();

    DL_SYSTICK_init(CPUCLK_FREQ / 1000U);
    DL_SYSTICK_enableInterrupt();
    DL_SYSTICK_enable();

    /* PA12、PA13 均从熄灭状态开始 */
    PwmOutput_SetDuty(
        LED_PWM_INST, GPIO_LED_PWM_C0_IDX, 0U);
    PwmOutput_SetDuty(
        LED_PWM_INST, GPIO_LED_PWM_C1_IDX, 0U);

    /* 两个 PWM 通道共用 TIMG0，只需启动一次 */
    PwmOutput_Start(LED_PWM_INST);

    while (1) {
        uint32_t now = g_msTicks;

        if (now != lastPwmUpdateTime) {
            uint32_t cycleTime;
            uint16_t dutyPA12;
            uint16_t dutyPA13;

            lastPwmUpdateTime = now;
            cycleTime = now % BREATH_CYCLE_MS;

            /*
             * PA12：
             *   0～1 s   占空比 0～1000，由暗变亮
             *   1～2 s   占空比 1000～0，由亮变暗
             *   2～4 s   保持熄灭
             */
            if (cycleTime <= BREATH_RAMP_MS) {
                dutyPA12 = (uint16_t) cycleTime;
            } else if (cycleTime < BREATH_HALF_CYCLE_MS) {
                dutyPA12 =
                    (uint16_t)(BREATH_HALF_CYCLE_MS - cycleTime);
            } else {
                dutyPA12 = 0U;
            }

            /*
             * PA13：
             *   0～2 s   保持熄灭
             *   2～3 s   占空比 0～1000，由暗变亮
             *   3～4 s   占空比 1000～0，由亮变暗
             */
            if (cycleTime < BREATH_HALF_CYCLE_MS) {
                dutyPA13 = 0U;
            } else if (cycleTime <=
                       (BREATH_HALF_CYCLE_MS + BREATH_RAMP_MS)) {
                dutyPA13 =
                    (uint16_t)(cycleTime - BREATH_HALF_CYCLE_MS);
            } else {
                dutyPA13 =
                    (uint16_t)(BREATH_CYCLE_MS - cycleTime);
            }

            PwmOutput_SetDuty(LED_PWM_INST, GPIO_LED_PWM_C0_IDX, dutyPA12);
            PwmOutput_SetDuty(LED_PWM_INST, GPIO_LED_PWM_C1_IDX, dutyPA13);
        }
    }
}
