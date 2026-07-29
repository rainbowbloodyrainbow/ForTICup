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



/* Defines for MOTOR_PWM */
#define MOTOR_PWM_INST                                                     TIMG0
#define MOTOR_PWM_INST_IRQHandler                               TIMG0_IRQHandler
#define MOTOR_PWM_INST_INT_IRQN                                 (TIMG0_INT_IRQn)
#define MOTOR_PWM_INST_CLK_FREQ                                         32000000
/* GPIO defines for channel 0 */
#define GPIO_MOTOR_PWM_C0_PORT                                             GPIOA
#define GPIO_MOTOR_PWM_C0_PIN                                     DL_GPIO_PIN_12
#define GPIO_MOTOR_PWM_C0_IOMUX                                  (IOMUX_PINCM34)
#define GPIO_MOTOR_PWM_C0_IOMUX_FUNC                 IOMUX_PINCM34_PF_TIMG0_CCP0
#define GPIO_MOTOR_PWM_C0_IDX                                DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_MOTOR_PWM_C1_PORT                                             GPIOA
#define GPIO_MOTOR_PWM_C1_PIN                                     DL_GPIO_PIN_13
#define GPIO_MOTOR_PWM_C1_IOMUX                                  (IOMUX_PINCM35)
#define GPIO_MOTOR_PWM_C1_IOMUX_FUNC                 IOMUX_PINCM35_PF_TIMG0_CCP1
#define GPIO_MOTOR_PWM_C1_IDX                                DL_TIMER_CC_1_INDEX

/* Defines for SERVO_PWM */
#define SERVO_PWM_INST                                                    TIMG12
#define SERVO_PWM_INST_IRQHandler                              TIMG12_IRQHandler
#define SERVO_PWM_INST_INT_IRQN                                (TIMG12_INT_IRQn)
#define SERVO_PWM_INST_CLK_FREQ                                         32000000
/* GPIO defines for channel 1 */
#define GPIO_SERVO_PWM_C1_PORT                                             GPIOB
#define GPIO_SERVO_PWM_C1_PIN                                     DL_GPIO_PIN_14
#define GPIO_SERVO_PWM_C1_IOMUX                                  (IOMUX_PINCM31)
#define GPIO_SERVO_PWM_C1_IOMUX_FUNC                IOMUX_PINCM31_PF_TIMG12_CCP1
#define GPIO_SERVO_PWM_C1_IDX                                DL_TIMER_CC_1_INDEX




/* Defines for MPU6050_I2C */
#define MPU6050_I2C_INST                                                    I2C0
#define MPU6050_I2C_INST_IRQHandler                              I2C0_IRQHandler
#define MPU6050_I2C_INST_INT_IRQN                                  I2C0_INT_IRQn
#define MPU6050_I2C_BUS_SPEED_HZ                                          400000
#define GPIO_MPU6050_I2C_SDA_PORT                                          GPIOA
#define GPIO_MPU6050_I2C_SDA_PIN                                   DL_GPIO_PIN_0
#define GPIO_MPU6050_I2C_IOMUX_SDA                                (IOMUX_PINCM1)
#define GPIO_MPU6050_I2C_IOMUX_SDA_FUNC                 IOMUX_PINCM1_PF_I2C0_SDA
#define GPIO_MPU6050_I2C_SCL_PORT                                          GPIOA
#define GPIO_MPU6050_I2C_SCL_PIN                                   DL_GPIO_PIN_1
#define GPIO_MPU6050_I2C_IOMUX_SCL                                (IOMUX_PINCM2)
#define GPIO_MPU6050_I2C_IOMUX_SCL_FUNC                 IOMUX_PINCM2_PF_I2C0_SCL


/* Defines for HC05_UART */
#define HC05_UART_INST                                                     UART1
#define HC05_UART_INST_FREQUENCY                                        32000000
#define HC05_UART_INST_IRQHandler                               UART1_IRQHandler
#define HC05_UART_INST_INT_IRQN                                   UART1_INT_IRQn
#define GPIO_HC05_UART_RX_PORT                                             GPIOA
#define GPIO_HC05_UART_TX_PORT                                             GPIOA
#define GPIO_HC05_UART_RX_PIN                                      DL_GPIO_PIN_9
#define GPIO_HC05_UART_TX_PIN                                      DL_GPIO_PIN_8
#define GPIO_HC05_UART_IOMUX_RX                                  (IOMUX_PINCM20)
#define GPIO_HC05_UART_IOMUX_TX                                  (IOMUX_PINCM19)
#define GPIO_HC05_UART_IOMUX_RX_FUNC                   IOMUX_PINCM20_PF_UART1_RX
#define GPIO_HC05_UART_IOMUX_TX_FUNC                   IOMUX_PINCM19_PF_UART1_TX
#define HC05_UART_BAUD_RATE                                             (115200)
#define HC05_UART_IBRD_32_MHZ_115200_BAUD                                   (17)
#define HC05_UART_FBRD_32_MHZ_115200_BAUD                                   (23)





/* Defines for LINE_ADC */
#define LINE_ADC_INST                                                       ADC0
#define LINE_ADC_INST_IRQHandler                                 ADC0_IRQHandler
#define LINE_ADC_INST_INT_IRQN                                   (ADC0_INT_IRQn)
#define LINE_ADC_ADCMEM_SENSOR_1                              DL_ADC12_MEM_IDX_0
#define LINE_ADC_ADCMEM_SENSOR_1_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_1_REF_VOLTAGE_V                                     3.3
#define LINE_ADC_ADCMEM_SENSOR_2                              DL_ADC12_MEM_IDX_1
#define LINE_ADC_ADCMEM_SENSOR_2_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_2_REF_VOLTAGE_V                                     3.3
#define LINE_ADC_ADCMEM_SENSOR_3                              DL_ADC12_MEM_IDX_2
#define LINE_ADC_ADCMEM_SENSOR_3_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_3_REF_VOLTAGE_V                                     3.3
#define LINE_ADC_ADCMEM_SENSOR_4                              DL_ADC12_MEM_IDX_3
#define LINE_ADC_ADCMEM_SENSOR_4_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_4_REF_VOLTAGE_V                                     3.3
#define LINE_ADC_ADCMEM_SENSOR_5                              DL_ADC12_MEM_IDX_4
#define LINE_ADC_ADCMEM_SENSOR_5_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_5_REF_VOLTAGE_V                                     3.3
#define LINE_ADC_ADCMEM_SENSOR_6                              DL_ADC12_MEM_IDX_5
#define LINE_ADC_ADCMEM_SENSOR_6_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_6_REF_VOLTAGE_V                                     3.3
#define LINE_ADC_ADCMEM_SENSOR_7                              DL_ADC12_MEM_IDX_6
#define LINE_ADC_ADCMEM_SENSOR_7_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_7_REF_VOLTAGE_V                                     3.3
#define LINE_ADC_ADCMEM_SENSOR_8                              DL_ADC12_MEM_IDX_7
#define LINE_ADC_ADCMEM_SENSOR_8_REF             DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define LINE_ADC_ADCMEM_SENSOR_8_REF_VOLTAGE_V                                     3.3
#define GPIO_LINE_ADC_C0_PORT                                              GPIOA
#define GPIO_LINE_ADC_C0_PIN                                      DL_GPIO_PIN_27
#define GPIO_LINE_ADC_IOMUX_C0                                   (IOMUX_PINCM60)
#define GPIO_LINE_ADC_IOMUX_C0_FUNC               (IOMUX_PINCM60_PF_UNCONNECTED)
#define GPIO_LINE_ADC_C1_PORT                                              GPIOA
#define GPIO_LINE_ADC_C1_PIN                                      DL_GPIO_PIN_26
#define GPIO_LINE_ADC_IOMUX_C1                                   (IOMUX_PINCM59)
#define GPIO_LINE_ADC_IOMUX_C1_FUNC               (IOMUX_PINCM59_PF_UNCONNECTED)
#define GPIO_LINE_ADC_C2_PORT                                              GPIOA
#define GPIO_LINE_ADC_C2_PIN                                      DL_GPIO_PIN_25
#define GPIO_LINE_ADC_IOMUX_C2                                   (IOMUX_PINCM55)
#define GPIO_LINE_ADC_IOMUX_C2_FUNC               (IOMUX_PINCM55_PF_UNCONNECTED)
#define GPIO_LINE_ADC_C3_PORT                                              GPIOA
#define GPIO_LINE_ADC_C3_PIN                                      DL_GPIO_PIN_24
#define GPIO_LINE_ADC_IOMUX_C3                                   (IOMUX_PINCM54)
#define GPIO_LINE_ADC_IOMUX_C3_FUNC               (IOMUX_PINCM54_PF_UNCONNECTED)
#define GPIO_LINE_ADC_C4_PORT                                              GPIOB
#define GPIO_LINE_ADC_C4_PIN                                      DL_GPIO_PIN_25
#define GPIO_LINE_ADC_IOMUX_C4                                   (IOMUX_PINCM56)
#define GPIO_LINE_ADC_IOMUX_C4_FUNC               (IOMUX_PINCM56_PF_UNCONNECTED)
#define GPIO_LINE_ADC_C5_PORT                                              GPIOB
#define GPIO_LINE_ADC_C5_PIN                                      DL_GPIO_PIN_24
#define GPIO_LINE_ADC_IOMUX_C5                                   (IOMUX_PINCM52)
#define GPIO_LINE_ADC_IOMUX_C5_FUNC               (IOMUX_PINCM52_PF_UNCONNECTED)
#define GPIO_LINE_ADC_C6_PORT                                              GPIOB
#define GPIO_LINE_ADC_C6_PIN                                      DL_GPIO_PIN_20
#define GPIO_LINE_ADC_IOMUX_C6                                   (IOMUX_PINCM48)
#define GPIO_LINE_ADC_IOMUX_C6_FUNC               (IOMUX_PINCM48_PF_UNCONNECTED)
#define GPIO_LINE_ADC_C7_PORT                                              GPIOA
#define GPIO_LINE_ADC_C7_PIN                                      DL_GPIO_PIN_22
#define GPIO_LINE_ADC_IOMUX_C7                                   (IOMUX_PINCM47)
#define GPIO_LINE_ADC_IOMUX_C7_FUNC               (IOMUX_PINCM47_PF_UNCONNECTED)



/* Defines for AIN1: GPIOB.10 with pinCMx 27 on package pin 62 */
#define MOTOR_CONTROL_AIN1_PORT                                          (GPIOB)
#define MOTOR_CONTROL_AIN1_PIN                                  (DL_GPIO_PIN_10)
#define MOTOR_CONTROL_AIN1_IOMUX                                 (IOMUX_PINCM27)
/* Defines for AIN2: GPIOB.11 with pinCMx 28 on package pin 63 */
#define MOTOR_CONTROL_AIN2_PORT                                          (GPIOB)
#define MOTOR_CONTROL_AIN2_PIN                                  (DL_GPIO_PIN_11)
#define MOTOR_CONTROL_AIN2_IOMUX                                 (IOMUX_PINCM28)
/* Defines for BIN1: GPIOB.12 with pinCMx 29 on package pin 64 */
#define MOTOR_CONTROL_BIN1_PORT                                          (GPIOB)
#define MOTOR_CONTROL_BIN1_PIN                                  (DL_GPIO_PIN_12)
#define MOTOR_CONTROL_BIN1_IOMUX                                 (IOMUX_PINCM29)
/* Defines for BIN2: GPIOB.13 with pinCMx 30 on package pin 1 */
#define MOTOR_CONTROL_BIN2_PORT                                          (GPIOB)
#define MOTOR_CONTROL_BIN2_PIN                                  (DL_GPIO_PIN_13)
#define MOTOR_CONTROL_BIN2_IOMUX                                 (IOMUX_PINCM30)
/* Defines for STBY: GPIOA.7 with pinCMx 14 on package pin 49 */
#define MOTOR_CONTROL_STBY_PORT                                          (GPIOA)
#define MOTOR_CONTROL_STBY_PIN                                   (DL_GPIO_PIN_7)
#define MOTOR_CONTROL_STBY_IOMUX                                 (IOMUX_PINCM14)
/* Port definition for Pin Group GPIO_ENCODER */
#define GPIO_ENCODER_PORT                                                (GPIOB)

/* Defines for LEFT_ENCODER_A: GPIOB.2 with pinCMx 15 on package pin 50 */
// pins affected by this interrupt request:["LEFT_ENCODER_A","LEFT_ENCODER_B","RIGHT_ENCODER_A","RIGHT_ENCODER_B"]
#define GPIO_ENCODER_INT_IRQN                                   (GPIOB_INT_IRQn)
#define GPIO_ENCODER_INT_IIDX                   (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define GPIO_ENCODER_LEFT_ENCODER_A_IIDX                     (DL_GPIO_IIDX_DIO2)
#define GPIO_ENCODER_LEFT_ENCODER_A_PIN                          (DL_GPIO_PIN_2)
#define GPIO_ENCODER_LEFT_ENCODER_A_IOMUX                        (IOMUX_PINCM15)
/* Defines for LEFT_ENCODER_B: GPIOB.3 with pinCMx 16 on package pin 51 */
#define GPIO_ENCODER_LEFT_ENCODER_B_IIDX                     (DL_GPIO_IIDX_DIO3)
#define GPIO_ENCODER_LEFT_ENCODER_B_PIN                          (DL_GPIO_PIN_3)
#define GPIO_ENCODER_LEFT_ENCODER_B_IOMUX                        (IOMUX_PINCM16)
/* Defines for RIGHT_ENCODER_A: GPIOB.4 with pinCMx 17 on package pin 52 */
#define GPIO_ENCODER_RIGHT_ENCODER_A_IIDX                    (DL_GPIO_IIDX_DIO4)
#define GPIO_ENCODER_RIGHT_ENCODER_A_PIN                         (DL_GPIO_PIN_4)
#define GPIO_ENCODER_RIGHT_ENCODER_A_IOMUX                       (IOMUX_PINCM17)
/* Defines for RIGHT_ENCODER_B: GPIOB.5 with pinCMx 18 on package pin 53 */
#define GPIO_ENCODER_RIGHT_ENCODER_B_IIDX                    (DL_GPIO_IIDX_DIO5)
#define GPIO_ENCODER_RIGHT_ENCODER_B_PIN                         (DL_GPIO_PIN_5)
#define GPIO_ENCODER_RIGHT_ENCODER_B_IOMUX                       (IOMUX_PINCM18)




/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_MOTOR_PWM_init(void);
void SYSCFG_DL_SERVO_PWM_init(void);
void SYSCFG_DL_MPU6050_I2C_init(void);
void SYSCFG_DL_HC05_UART_init(void);
void SYSCFG_DL_LINE_ADC_init(void);

void SYSCFG_DL_SYSTICK_init(void);

bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
