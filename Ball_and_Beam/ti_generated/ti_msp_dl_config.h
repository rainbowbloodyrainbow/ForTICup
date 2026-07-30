/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define CPUCLK_FREQ                                                     32000000



/* Defines for FOC_PWM */
#define FOC_PWM_INST                                                       TIMA0
#define FOC_PWM_INST_IRQHandler                                 TIMA0_IRQHandler
#define FOC_PWM_INST_INT_IRQN                                   (TIMA0_INT_IRQn)
#define FOC_PWM_INST_CLK_FREQ                                           32000000
/* GPIO defines for channel 0 */
#define GPIO_FOC_PWM_C0_PORT                                               GPIOA
#define GPIO_FOC_PWM_C0_PIN                                       DL_GPIO_PIN_21
#define GPIO_FOC_PWM_C0_IOMUX                                    (IOMUX_PINCM46)
#define GPIO_FOC_PWM_C0_IOMUX_FUNC                   IOMUX_PINCM46_PF_TIMA0_CCP0
#define GPIO_FOC_PWM_C0_IDX                                  DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_FOC_PWM_C1_PORT                                               GPIOA
#define GPIO_FOC_PWM_C1_PIN                                       DL_GPIO_PIN_22
#define GPIO_FOC_PWM_C1_IOMUX                                    (IOMUX_PINCM47)
#define GPIO_FOC_PWM_C1_IOMUX_FUNC                   IOMUX_PINCM47_PF_TIMA0_CCP1
#define GPIO_FOC_PWM_C1_IDX                                  DL_TIMER_CC_1_INDEX
/* GPIO defines for channel 3 */
#define GPIO_FOC_PWM_C3_PORT                                               GPIOA
#define GPIO_FOC_PWM_C3_PIN                                       DL_GPIO_PIN_23
#define GPIO_FOC_PWM_C3_IOMUX                                    (IOMUX_PINCM53)
#define GPIO_FOC_PWM_C3_IOMUX_FUNC                   IOMUX_PINCM53_PF_TIMA0_CCP3
#define GPIO_FOC_PWM_C3_IDX                                  DL_TIMER_CC_3_INDEX



/* Defines for CONTROL_TICK */
#define CONTROL_TICK_INST                                                (TIMG0)
#define CONTROL_TICK_INST_IRQHandler                            TIMG0_IRQHandler
#define CONTROL_TICK_INST_INT_IRQN                              (TIMG0_INT_IRQn)
#define CONTROL_TICK_INST_LOAD_VALUE                                      (999U)




/* Defines for AS5600_I2C */
#define AS5600_I2C_INST                                                     I2C0
#define AS5600_I2C_INST_IRQHandler                               I2C0_IRQHandler
#define AS5600_I2C_INST_INT_IRQN                                   I2C0_INT_IRQn
#define AS5600_I2C_BUS_SPEED_HZ                                           400000
#define GPIO_AS5600_I2C_SDA_PORT                                           GPIOA
#define GPIO_AS5600_I2C_SDA_PIN                                    DL_GPIO_PIN_0
#define GPIO_AS5600_I2C_IOMUX_SDA                                 (IOMUX_PINCM1)
#define GPIO_AS5600_I2C_IOMUX_SDA_FUNC                  IOMUX_PINCM1_PF_I2C0_SDA
#define GPIO_AS5600_I2C_SCL_PORT                                           GPIOA
#define GPIO_AS5600_I2C_SCL_PIN                                    DL_GPIO_PIN_1
#define GPIO_AS5600_I2C_IOMUX_SCL                                 (IOMUX_PINCM2)
#define GPIO_AS5600_I2C_IOMUX_SCL_FUNC                  IOMUX_PINCM2_PF_I2C0_SCL


/* Defines for CMD_UART */
#define CMD_UART_INST                                                      UART1
#define CMD_UART_INST_FREQUENCY                                         32000000
#define CMD_UART_INST_IRQHandler                                UART1_IRQHandler
#define CMD_UART_INST_INT_IRQN                                    UART1_INT_IRQn
#define GPIO_CMD_UART_RX_PORT                                              GPIOA
#define GPIO_CMD_UART_TX_PORT                                              GPIOA
#define GPIO_CMD_UART_RX_PIN                                       DL_GPIO_PIN_9
#define GPIO_CMD_UART_TX_PIN                                       DL_GPIO_PIN_8
#define GPIO_CMD_UART_IOMUX_RX                                   (IOMUX_PINCM20)
#define GPIO_CMD_UART_IOMUX_TX                                   (IOMUX_PINCM19)
#define GPIO_CMD_UART_IOMUX_RX_FUNC                    IOMUX_PINCM20_PF_UART1_RX
#define GPIO_CMD_UART_IOMUX_TX_FUNC                    IOMUX_PINCM19_PF_UART1_TX
#define CMD_UART_BAUD_RATE                                              (115200)
#define CMD_UART_IBRD_32_MHZ_115200_BAUD                                    (17)
#define CMD_UART_FBRD_32_MHZ_115200_BAUD                                    (23)





/* Port definition for Pin Group FOC_GPIO */
#define FOC_GPIO_PORT                                                    (GPIOA)

/* Defines for EN: GPIOA.24 with pinCMx 54 on package pin 44 */
#define FOC_GPIO_EN_PIN                                         (DL_GPIO_PIN_24)
#define FOC_GPIO_EN_IOMUX                                        (IOMUX_PINCM54)
/* Defines for NFAULT: GPIOA.25 with pinCMx 55 on package pin 45 */
#define FOC_GPIO_NFAULT_PIN                                     (DL_GPIO_PIN_25)
#define FOC_GPIO_NFAULT_IOMUX                                    (IOMUX_PINCM55)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_FOC_PWM_init(void);
void SYSCFG_DL_CONTROL_TICK_init(void);
void SYSCFG_DL_AS5600_I2C_init(void);
void SYSCFG_DL_CMD_UART_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
