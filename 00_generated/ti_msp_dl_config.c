/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_MOTOR_PWM_init();
    SYSCFG_DL_SERVO_PWM_init();
    SYSCFG_DL_MPU6050_I2C_init();
    SYSCFG_DL_HC05_UART_init();
    SYSCFG_DL_LINE_ADC_init();
    SYSCFG_DL_SYSTICK_init();
}



SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerG_reset(MOTOR_PWM_INST);
    DL_TimerG_reset(SERVO_PWM_INST);
    DL_I2C_reset(MPU6050_I2C_INST);
    DL_UART_Main_reset(HC05_UART_INST);
    DL_ADC12_reset(LINE_ADC_INST);


    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerG_enablePower(MOTOR_PWM_INST);
    DL_TimerG_enablePower(SERVO_PWM_INST);
    DL_I2C_enablePower(MPU6050_I2C_INST);
    DL_UART_Main_enablePower(HC05_UART_INST);
    DL_ADC12_enablePower(LINE_ADC_INST);

    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralOutputFunction(GPIO_MOTOR_PWM_C0_IOMUX,GPIO_MOTOR_PWM_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_MOTOR_PWM_C0_PORT, GPIO_MOTOR_PWM_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_MOTOR_PWM_C1_IOMUX,GPIO_MOTOR_PWM_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_MOTOR_PWM_C1_PORT, GPIO_MOTOR_PWM_C1_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_SERVO_PWM_C1_IOMUX,GPIO_SERVO_PWM_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_SERVO_PWM_C1_PORT, GPIO_SERVO_PWM_C1_PIN);

    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_MPU6050_I2C_IOMUX_SDA,
        GPIO_MPU6050_I2C_IOMUX_SDA_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initPeripheralInputFunctionFeatures(GPIO_MPU6050_I2C_IOMUX_SCL,
        GPIO_MPU6050_I2C_IOMUX_SCL_FUNC, DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_NONE, DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_MPU6050_I2C_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_MPU6050_I2C_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_HC05_UART_IOMUX_TX, GPIO_HC05_UART_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_HC05_UART_IOMUX_RX, GPIO_HC05_UART_IOMUX_RX_FUNC);

    DL_GPIO_initDigitalOutput(MOTOR_CONTROL_AIN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_CONTROL_AIN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_CONTROL_BIN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_CONTROL_BIN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_CONTROL_STBY_IOMUX);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODER_LEFT_ENCODER_A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODER_LEFT_ENCODER_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODER_RIGHT_ENCODER_A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(GPIO_ENCODER_RIGHT_ENCODER_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_clearPins(GPIOA, MOTOR_CONTROL_STBY_PIN);
    DL_GPIO_enableOutput(GPIOA, MOTOR_CONTROL_STBY_PIN);
    DL_GPIO_clearPins(GPIOB, MOTOR_CONTROL_AIN1_PIN |
		MOTOR_CONTROL_AIN2_PIN |
		MOTOR_CONTROL_BIN1_PIN |
		MOTOR_CONTROL_BIN2_PIN);
    DL_GPIO_enableOutput(GPIOB, MOTOR_CONTROL_AIN1_PIN |
		MOTOR_CONTROL_AIN2_PIN |
		MOTOR_CONTROL_BIN1_PIN |
		MOTOR_CONTROL_BIN2_PIN);
    DL_GPIO_setLowerPinsPolarity(GPIOB, DL_GPIO_PIN_2_EDGE_RISE_FALL |
		DL_GPIO_PIN_3_EDGE_RISE_FALL |
		DL_GPIO_PIN_4_EDGE_RISE_FALL |
		DL_GPIO_PIN_5_EDGE_RISE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOB, GPIO_ENCODER_LEFT_ENCODER_A_PIN |
		GPIO_ENCODER_LEFT_ENCODER_B_PIN |
		GPIO_ENCODER_RIGHT_ENCODER_A_PIN |
		GPIO_ENCODER_RIGHT_ENCODER_B_PIN);
    DL_GPIO_enableInterrupt(GPIOB, GPIO_ENCODER_LEFT_ENCODER_A_PIN |
		GPIO_ENCODER_LEFT_ENCODER_B_PIN |
		GPIO_ENCODER_RIGHT_ENCODER_A_PIN |
		GPIO_ENCODER_RIGHT_ENCODER_B_PIN);

}


SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);

    DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
    /* Set default configuration */
    DL_SYSCTL_disableHFXT();
    DL_SYSCTL_disableSYSPLL();
    DL_SYSCTL_setULPCLKDivider(DL_SYSCTL_ULPCLK_DIV_1);
    DL_SYSCTL_setMCLKDivider(DL_SYSCTL_MCLK_DIVIDER_DISABLE);

}


/*
 * Timer clock configuration to be sourced by  / 1 (32000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   32000000 Hz = 32000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerG_ClockConfig gMOTOR_PWMClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerG_PWMConfig gMOTOR_PWMConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 1600,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_MOTOR_PWM_init(void) {

    DL_TimerG_setClockConfig(
        MOTOR_PWM_INST, (DL_TimerG_ClockConfig *) &gMOTOR_PWMClockConfig);

    DL_TimerG_initPWMMode(
        MOTOR_PWM_INST, (DL_TimerG_PWMConfig *) &gMOTOR_PWMConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerG_setCounterControl(MOTOR_PWM_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerG_setCaptureCompareOutCtl(MOTOR_PWM_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_0_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(MOTOR_PWM_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, 1600, DL_TIMER_CC_0_INDEX);

    DL_TimerG_setCaptureCompareOutCtl(MOTOR_PWM_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_1_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(MOTOR_PWM_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_1_INDEX);
    DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, 1600, DL_TIMER_CC_1_INDEX);

    DL_TimerG_enableClock(MOTOR_PWM_INST);


    
    DL_TimerG_setCCPDirection(MOTOR_PWM_INST , DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT );


}
/*
 * Timer clock configuration to be sourced by  / 1 (32000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   32000000 Hz = 32000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerG_ClockConfig gSERVO_PWMClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerG_PWMConfig gSERVO_PWMConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 640000,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_STOP,
};

SYSCONFIG_WEAK void SYSCFG_DL_SERVO_PWM_init(void) {

    DL_TimerG_setClockConfig(
        SERVO_PWM_INST, (DL_TimerG_ClockConfig *) &gSERVO_PWMClockConfig);

    DL_TimerG_initPWMMode(
        SERVO_PWM_INST, (DL_TimerG_PWMConfig *) &gSERVO_PWMConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerG_setCounterControl(SERVO_PWM_INST,DL_TIMER_CZC_CCCTL1_ZCOND,DL_TIMER_CAC_CCCTL1_ACOND,DL_TIMER_CLC_CCCTL1_LCOND);

    DL_TimerG_setCaptureCompareOutCtl(SERVO_PWM_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERG_CAPTURE_COMPARE_1_INDEX);

    DL_TimerG_setCaptCompUpdateMethod(SERVO_PWM_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERG_CAPTURE_COMPARE_1_INDEX);
    DL_TimerG_setCaptureCompareValue(SERVO_PWM_INST, 640000, DL_TIMER_CC_1_INDEX);

    DL_TimerG_enableClock(SERVO_PWM_INST);


    
    DL_TimerG_setCCPDirection(SERVO_PWM_INST , DL_TIMER_CC1_OUTPUT );


}


static const DL_I2C_ClockConfig gMPU6050_I2CClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_MPU6050_I2C_init(void) {

    DL_I2C_setClockConfig(MPU6050_I2C_INST,
        (DL_I2C_ClockConfig *) &gMPU6050_I2CClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(MPU6050_I2C_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(MPU6050_I2C_INST);

    /* Configure Controller Mode */
    DL_I2C_resetControllerTransfer(MPU6050_I2C_INST);
    /* Set frequency to 400000 Hz*/
    DL_I2C_setTimerPeriod(MPU6050_I2C_INST, 7);
    DL_I2C_setControllerTXFIFOThreshold(MPU6050_I2C_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setControllerRXFIFOThreshold(MPU6050_I2C_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableControllerClockStretching(MPU6050_I2C_INST);

    /* Configure Interrupts */
    DL_I2C_enableInterrupt(MPU6050_I2C_INST,
                           DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST |
                           DL_I2C_INTERRUPT_CONTROLLER_NACK |
                           DL_I2C_INTERRUPT_CONTROLLER_RXFIFO_TRIGGER |
                           DL_I2C_INTERRUPT_CONTROLLER_RX_DONE |
                           DL_I2C_INTERRUPT_CONTROLLER_TX_DONE);


    /* Enable module */
    DL_I2C_enableController(MPU6050_I2C_INST);


}

static const DL_UART_Main_ClockConfig gHC05_UARTClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gHC05_UARTConfig = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_HC05_UART_init(void)
{
    DL_UART_Main_setClockConfig(HC05_UART_INST, (DL_UART_Main_ClockConfig *) &gHC05_UARTClockConfig);

    DL_UART_Main_init(HC05_UART_INST, (DL_UART_Main_Config *) &gHC05_UARTConfig);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(HC05_UART_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(HC05_UART_INST, HC05_UART_IBRD_32_MHZ_115200_BAUD, HC05_UART_FBRD_32_MHZ_115200_BAUD);


    /* Configure Interrupts */
    DL_UART_Main_enableInterrupt(HC05_UART_INST,
                                 DL_UART_MAIN_INTERRUPT_RX);


    DL_UART_Main_enable(HC05_UART_INST);
}

/* LINE_ADC Initialization */
static const DL_ADC12_ClockConfig gLINE_ADCClockConfig = {
    .clockSel       = DL_ADC12_CLOCK_ULPCLK,
    .divideRatio    = DL_ADC12_CLOCK_DIVIDE_8,
    .freqRange      = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};
SYSCONFIG_WEAK void SYSCFG_DL_LINE_ADC_init(void)
{
    DL_ADC12_setClockConfig(LINE_ADC_INST, (DL_ADC12_ClockConfig *) &gLINE_ADCClockConfig);

    DL_ADC12_initSeqSample(LINE_ADC_INST,
        DL_ADC12_REPEAT_MODE_DISABLED, DL_ADC12_SAMPLING_SOURCE_AUTO, DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SEQ_START_ADDR_00, DL_ADC12_SEQ_END_ADDR_07, DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_1,
        DL_ADC12_INPUT_CHAN_0, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_2,
        DL_ADC12_INPUT_CHAN_1, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_3,
        DL_ADC12_INPUT_CHAN_2, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_4,
        DL_ADC12_INPUT_CHAN_3, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_5,
        DL_ADC12_INPUT_CHAN_4, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_6,
        DL_ADC12_INPUT_CHAN_5, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_7,
        DL_ADC12_INPUT_CHAN_6, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_configConversionMem(LINE_ADC_INST, LINE_ADC_ADCMEM_SENSOR_8,
        DL_ADC12_INPUT_CHAN_7, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_setPowerDownMode(LINE_ADC_INST,DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(LINE_ADC_INST,500);
    DL_ADC12_enableConversions(LINE_ADC_INST);
}

SYSCONFIG_WEAK void SYSCFG_DL_SYSTICK_init(void)
{
    /*
     * Initializes the SysTick period to 1.00 ms,
     * enables the interrupt, and starts the SysTick Timer
     */
    DL_SYSTICK_config(32000);
}

