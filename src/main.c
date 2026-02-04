#include <ch32x035.h> /* both X033 and X035 */
#include <stdio.h>

#include "debug.h"

// analog pins (PA0-PA7, PB0-PB1, PC0-PC3 : 13 channels)
#define AIN1_PORT               GPIOA // PA0: Ain1
#define AIN1_PIN                GPIO_Pin_0
#define AIN1_CHANNEL            ADC_Channel_0
#define AIN0_PORT               GPIOA // PA1: Ain0
#define AIN0_PIN                GPIO_Pin_1
#define AIN0_CHANNEL            ADC_Channel_1
#define BATTERY_MONITOR_PORT    GPIOC // PC0: Battery Monitor
#define BATTERY_MONITOR_PIN     GPIO_Pin_0
#define BATTERY_MONITOR_CHANNEL ADC_Channel_9
#define USB_MONITOR_PORT        GPIOC // PC3: USB Monitor
#define USB_MONITOR_PIN         GPIO_Pin_0
#define USB_MONITOR_CHANNEL     ADC_Channel_12
#define JOYSTICK_Y_PORT         GPIOA // PA5: JoyY
#define JOYSTICK_Y_PIN          GPIO_Pin_5
#define JOYSTICK_Y_CHANNEL      ADC_Channel_4
#define JOYSTICK_X_PORT         GPIOA // PA6: JoyX
#define JOYSTICK_X_PIN          GPIO_Pin_6
#define JOYSTICK_X_CHANNEL      ADC_Channel_5
#define ADC_CHANNELS            (7) // 6 inputs + Vref

// digital inputs
#define CHARGER_CHARGING_PORT GPIOA // PA2: Charger, charging
#define CHARGER_CHARGING_PIN  GPIO_Pin_2
#define CHARGER_STANDBY_PORT  GPIOA // PA3: Charger, standby
#define CHARGER_STANDBY_PIN   GPIO_Pin_3
#define BUTTON_X_PORT         GPIOB // PB7: X Button
#define BUTTON_X_PIN          GPIO_Pin_7
#define BUTTON_A_PORT         GPIOB // PB8: A Button
#define BUTTON_A_PIN          GPIO_Pin_8
#define BUTTON_B_PORT         GPIOB // PB9: B Button
#define BUTTON_B_PIN          GPIO_Pin_9
#define BUTTON_Y_PORT         GPIOB // PB10: Y Button
#define BUTTON_Y_PIN          GPIO_Pin_10
#define BUTTON_MENU_PORT      GPIOC // PC15: Menu Button
#define BUTTON_MENU_PIN       GPIO_Pin_15

// digital outputs
#define AUX_POWER_PORT  GPIOB // PB6: Aux power
#define AUX_POWER_PIN   GPIO_Pin_6
#define LCD_RESET_PORT  GPIOB // PB11: LCD reset
#define LCD_RESET_PIN   GPIO_Pin_11
#define INT_OUTPUT_PORT GPIOC // PC16: Interrupt output
#define INT_OUTPUT_PIN  GPIO_Pin_16

// PWM
#define DEBUG_LED_PORT     GPIOA // PA4: debug LED
#define DEBUG_LED_PIN      GPIO_Pin_4
#define DEBUG_LED_TIM      TIM3 // mapping 0b11
#define DEBUG_LED_CHAN     TIM_Channel_2
#define BUZZER_PORT        GPIOC // PC14: Buzzer
#define BUZZER_PIN         GPIO_Pin_14
#define BUZZER_TIM         TIM2 // mapping 0b11x
#define BUZZER_CHAN        TIM_Channel_2
#define LCD_BACKLIGHT_PORT GPIOB // PB12: LCD BL
#define LCD_BACKLIGHT_PIN  GPIO_Pin_12
#define LCD_BACKLIGHT_TIM  TIM1 // mapping 0b010
#define LCD_BACKLIGHT_CHAN TIM_Channel_4

// I2C
#define SDA_PORT         GPIOC
#define SDA_PIN          GPIO_Pin_18
#define SCL_PORT         GPIOC
#define SCL_PIN          GPIO_Pin_19
#define I2C_ADDRESS      (0x38)
#define I2C_TIMEOUT      (-2)
#define I2C_TIMEOUT_TICK (1000)

static uint I2C_REG_ptr = 0;
static uint8_t test_buffer[] = {
    0x01,
    0x02,
    0x03,
};

static void Gpio_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    // outputs
    GPIO_InitStructure.GPIO_Pin = AUX_POWER_PIN | LCD_RESET_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = INT_OUTPUT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(INT_OUTPUT_PORT, &GPIO_InitStructure);

    // TODO: make this a PWM
    GPIO_InitStructure.GPIO_Pin = LCD_BACKLIGHT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_BACKLIGHT_PORT, &GPIO_InitStructure);

    // TODO: make this a PWM
    GPIO_InitStructure.GPIO_Pin = DEBUG_LED_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DEBUG_LED_PORT, &GPIO_InitStructure);

    // // inputs
    // GPIO_InitStructure.GPIO_Pin = CHARGER_CHARGING_PIN | CHARGER_STANDBY_PIN;
    // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    // GPIO_Init(GPIOA, &GPIO_InitStructure);

    // GPIO_InitStructure.GPIO_Pin = BUTTON_X_PIN | BUTTON_A_PIN | BUTTON_B_PIN | BUTTON_Y_PIN;
    // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    // GPIO_Init(GPIOB, &GPIO_InitStructure);

    // GPIO_InitStructure.GPIO_Pin = BUTTON_MENU_PIN;
    // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    // GPIO_Init(BUTTON_MENU_PORT, &GPIO_InitStructure);

    // // Setup interrupts
    // GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource2 | GPIO_PinSource3);
    // GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource7 | GPIO_PinSource8 | GPIO_PinSource9 | GPIO_PinSource10);
    // GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource15);
    // EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    // EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    // EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    // EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    // EXTI_Init(&EXTI_InitStructure);

    // NVIC_InitStructure.NVIC_IRQChannel = EXTI7_0_IRQn;
    // NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    // NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    // NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    // NVIC_Init(&NVIC_InitStructure);

    // NVIC_EnableIRQ(EXTI7_0_IRQn);
}

static void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef I2C_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStruct = {0};

    /* enable I2C1 and GPIOC clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    /* remap PC18/PC19 to I2C1 SDA/SCL */
    GPIO_PinRemapConfig(GPIO_PartialRemap3_I2C1, ENABLE); // 011: Mapping (SCL/PC19, SDA/PC18)

    /* Disable DIO (SWD) interface on these pins */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    /* configure the GPIO as SDA/SCL pins */
    GPIO_InitStructure.GPIO_Pin = SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // automatic open-drain
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SDA_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SCL_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // automatic open-drain
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SCL_PORT, &GPIO_InitStructure);

    /* configure I2C1 */
    I2C_InitStructure.I2C_ClockSpeed = 400000;                                // bus speed
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;                                // there is only 1 mode
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_16_9;                     // I2C fast mode Tlow/Thigh = 16/9
    I2C_InitStructure.I2C_OwnAddress1 = I2C_ADDRESS << 1;                     // 7 or 10 bit address
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;                               // automatic acknowledge
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit; // use 7 bit address
    I2C_Init(I2C1, &I2C_InitStructure);

    /* configure I2C interrupts */
    NVIC_InitStruct.NVIC_IRQChannel = I2C1_EV_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    NVIC_InitStruct.NVIC_IRQChannel = I2C1_ER_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    /* enable I2C interrupts */
    I2C_ITConfig(I2C1, I2C_IT_EVT | I2C_IT_ERR | I2C_IT_BUF, ENABLE); // TODO: also I2C_IT_BUF?

    /* enable clock stretching */
    I2C_StretchClockCmd(I2C1, ENABLE);
    /* enable I2C1 */
    I2C_Cmd(I2C1, ENABLE);
}

// static void PWM_Init(void)
// {
//     GPIO_InitTypeDef GPIO_InitStructure = {0};
//     TIM_OCInitTypeDef TIM_OCInitStructure = {0};
//     TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};

//     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);
//     GPIO_InitStructure.GPIO_Pin = DEBUG_LED_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
//     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//     GPIO_Init(DEBUG_LED_PORT, &GPIO_InitStructure);

//     // GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
//     // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
//     // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//     // GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);

//     GPIO_InitStructure.GPIO_Pin = LCD_BACKLIGHT_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
//     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//     GPIO_Init(LCD_BACKLIGHT_PORT, &GPIO_InitStructure);

//     // Enable timers
//     RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
//     RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3, ENABLE);

//     // set pinmux
//     GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);     // set debug LED (PA4) on CH2 of TIM3
//     GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, ENABLE);     // set Buzzer (PC14) on CH2 of TIM2
//     GPIO_PinRemapConfig(GPIO_PartialRemap2_TIM1, ENABLE); // set LCD backlight (PB12) on CH4 of TIM1

//     // TIM1 : LCD backlight
//     TIM_TimeBaseInitStructure.TIM_Period = 100;
//     TIM_TimeBaseInitStructure.TIM_Prescaler = 480 - 1;
//     TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
//     TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
//     TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);

// #if (PWM_MODE == PWM_MODE1)
//     TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
// #elif (PWM_MODE == PWM_MODE2)
//     TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
// #endif

//     TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//     TIM_OCInitStructure.TIM_Pulse = 75;
//     TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
//     TIM_OC1Init(TIM1, &TIM_OCInitStructure);

//     TIM_CtrlPWMOutputs(TIM1, ENABLE);
//     TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Disable);
//     TIM_ARRPreloadConfig(TIM1, ENABLE);
//     TIM_Cmd(TIM1, ENABLE);

//     // TIM2: Buzzer
//     TIM_TimeBaseInitStructure.TIM_Period = 100;
//     TIM_TimeBaseInitStructure.TIM_Prescaler = 480 - 1;
//     TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
//     TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
//     TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

// #if (PWM_MODE == PWM_MODE1)
//     TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
// #elif (PWM_MODE == PWM_MODE2)
//     TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
// #endif

//     TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//     TIM_OCInitStructure.TIM_Pulse = 75;
//     TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
//     TIM_OC1Init(TIM2, &TIM_OCInitStructure);

//     TIM_CtrlPWMOutputs(TIM2, ENABLE);
//     TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);
//     TIM_ARRPreloadConfig(TIM2, ENABLE);
//     TIM_Cmd(TIM2, ENABLE);

//     // TIM3 : debug LED
//     TIM_TimeBaseInitStructure.TIM_Period = 100;
//     TIM_TimeBaseInitStructure.TIM_Prescaler = 480 - 1;
//     TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
//     TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
//     TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

// #if (PWM_MODE == PWM_MODE1)
//     TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
// #elif (PWM_MODE == PWM_MODE2)
//     TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
// #endif

//     TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
//     TIM_OCInitStructure.TIM_Pulse = 75;
//     TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
//     TIM_OC1Init(TIM3, &TIM_OCInitStructure);

//     TIM_CtrlPWMOutputs(TIM3, ENABLE);
//     TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Disable);
//     TIM_ARRPreloadConfig(TIM3, ENABLE);
//     TIM_Cmd(TIM3, ENABLE);
// }

// static void Buzzer_PWM_Init(void)
// {
//     DMA_InitTypeDef DMA_InitStructure = {0};

//     // Enable GPIOC and Timer 1
//     RCC_APB2PeriphClockCmd(CC_APB2Periph_TIM1 | RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);
//     // Enable DMA
//     RCC_AHBPeriphClockCmd(RCC_AHBPeriph_SRAM | RCC_AHBPeriph_DMA1, ENABLE);

//     // PC3 is TIM1 CH3
//     GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
//     GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
//     GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);

//     DMA_DeInit(DMA1_Channel1);
//     DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&TIM1->CH3CVR; // This is T1CH2 Compare Register.
//     DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)s_buffer;
//     DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST; // MEM2PERIPHERAL
//     DMA_InitStructure.DMA_BufferSize = SAMPLES * 2;    // Number of samples to transfer;
//     DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
//     DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;                 // Increase memory.
//     DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word; // 32-bit peripheral
//     DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;     // 16-bit memory
//     DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;                         // Circular mode.
//     DMA_InitStructure.DMA_Priority = DMA_Priority_High;                     // High priority.
//     DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
//     DMA_Init(DMA1_Channel1, &DMA_InitStructure);

//     // configure interrupts
//     DMA_ITConfig(DMA1_Channel1, DMA_IT_HT | DMA_IT_TC, ENABLE); // Half-trigger, Whole-trigger

//     // Enable interrupts
//     NVIC_EnableIRQ(DMA_IRQn);

//     // Enable
//     DMA_Cmd(DMA1_Channel1, ENABLE);

//     // Reset TIM1 to init all regs
//     RCC_APB2PeriphResetCmd(RCC_APB2Periph_TIM1, ENABLE);
//     RCC_APB2PeriphResetCmd(RCC_APB2Periph_TIM1, DISABLE);

//     // CTLR1: default is up, events generated, edge align
//     // SMCFGR: default clk input is CK_INT

//     TIM_TimeBaseInitTypeDef TIM_InitStructure = {0};
//     TIM_InitStructure.TIM_Period = FREQ_TO_TICKS(SAMPLE_RATE); // Auto Reload - sets period
//     TIM_InitStructure.TIM_Prescaler = 0;                       // No prescaler, TIM1 clock is 48 MHz

//     TIM_TimeBaseInit(TIM1, &TIM_InitStructure);

//     // TODO
//     TIM_PrescalerConfig(TIM1, 0, TIM_PSCReloadMode_Immediate | TIM_PSCReloadMode_Update | TIM_EventSource_Trigger | TIM_EventSource_Update); // Update and trigger DMA

//     TIM_OCInitTypeDef TIM_OCInitStruct = {0};
//     TIM_OCStructInit(&TIM_OCInitStruct);

//     TIM_OCInitStruct.TIM_OC1Init(TIM1, TIM_OCNPolarity);

//     // Trigger DMA on update event
//     TIM_ITConfig(TIM1, TIM_IT_Update);

//     // RCC->APB2PCENR |= RCC_APB2Periph_TIM1 | RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
//     // // Enable DMA
//     // RCC->AHBPCENR = RCC_AHBPeriph_SRAM | RCC_AHBPeriph_DMA1;

//     // // PC3 is TIM1 CH3
//     // funPinMode(PC3, GPIO_CFGLR_OUT_10Mhz_AF_PP);

//     // // NOTE: The system can only DMA out at ~2.2MSPS.  2MHz is stable.
//     // // The idea here is that this copies, byte-at-a-time from the memory
//     // // into the peripheral addres.
//     // DMA_CHANNEL->CNTR = SAMPLES * 2; // Number of samples to transfer
//     // DMA_CHANNEL->MADDR = (uint32_t)s_buffer;
//     // DMA_CHANNEL->PADDR = (uint32_t)&TIM1->CH3CVR; // This is T1CH2 Compare Register.
//     // DMA_CHANNEL->CFGR =
//     //     DMA_CFGR1_DIR |     // MEM2PERIPHERAL
//     //     DMA_CFGR1_PL |      // High priority.
//     //     DMA_CFGR1_MSIZE_0 | // 16-bit memory
//     //     DMA_CFGR1_PSIZE_1 | // 32-bit peripheral
//     //     DMA_CFGR1_MINC |    // Increase memory.
//     //     DMA_CFGR1_CIRC |    // Circular mode.
//     //     DMA_CFGR1_HTIE |    // Half-trigger
//     //     DMA_CFGR1_TCIE |    // Whole-trigger
//     //     DMA_CFGR1_EN;       // Enable

//     // NVIC_EnableIRQ(DMA_IRQn);
//     // DMA_CHANNEL->CFGR |= DMA_CFGR1_EN;

//     // // Reset TIM1 to init all regs
//     // RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
//     // RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;

//     // // CTLR1: default is up, events generated, edge align
//     // // SMCFGR: default clk input is CK_INT

//     // // Prescaler
//     // TIM1->PSC = 0x0000; // No prescaler, TIM1 clock is 48 MHz

//     // // Auto Reload - sets period
//     // TIM1->ATRLR = FREQ_TO_TICKS(SAMPLE_RATE);

//     // // Reload immediately
//     // TIM1->SWEVGR |= TIM1_SWEVGR_TG | TIM1_SWEVGR_UG; // Update and trigger DMA

//     // // Enable CH2 output, normal polarity
//     // TIM1->CCER |= TIM1_CCER_CC3E; //| TIM1_CCER_CC3P;

//     // // CH3 Mode is output, PWM1 (CC2S = 00, OC2M = 110)
//     // TIM1->CHCTLR2 |= TIM1_CHCTLR2_OC3M_2 | TIM1_CHCTLR2_OC3M_1;

//     // // Set the Capture Compare Register value to off
//     // TIM1->CH3CVR = 0; // TIM1->ATRLR / 2; // Set to 50% duty cycle initially

//     // // TRGO on update event
//     // TIM1->CTLR2 = TIM1_CTLR2_MMS_1;

//     // // Enable TIM1 outputs
//     // TIM1->BDTR |= TIM1_BDTR_MOE;

//     // TIM1->DMAINTENR |= TIM1_DMAINTENR_UDE | TIM1_DMAINTENR_CC3DE; // Trigger DMA on update event

//     // // Enable TIM1
//     // TIM1->CTLR1 |= TIM1_CTLR1_CEN;
// }

// static void ADC_MultiChannel_Init(void)
// {
//     // https://curiousscientist.tech/blog/ch32v003f4p6-adc-basics
//     ADC_InitTypeDef ADC_InitStructure;
//     GPIO_InitTypeDef GPIO_InitStructure;

//     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
//     RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
//     ADC_CLKConfig(ADC1, ADC_CLK_Div8); // TODO?

//     GPIO_InitStructure.GPIO_Pin = BATTERY_MONITOR_PIN | USB_MONITOR_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//     GPIO_Init(GPIOC, &GPIO_InitStructure);

//     GPIO_InitStructure.GPIO_Pin = JOYSTICK_X_PIN | JOYSTICK_Y_PIN | AIN0_PIN | AIN1_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//     GPIO_Init(GPIOA, &GPIO_InitStructure);

//     ADC_DeInit(ADC1);

//     ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
//     ADC_InitStructure.ADC_ScanConvMode = ENABLE;
//     ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
//     ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
//     ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
//     ADC_InitStructure.ADC_NbrOfChannel = ADC_CHANNELS;
//     ADC_Init(ADC1, &ADC_InitStructure);

//     // TODO: check sampletime
//     ADC_RegularChannelConfig(ADC1, BATTERY_MONITOR_CHANNEL, 1, ADC_SampleTime_4Cycles);
//     ADC_RegularChannelConfig(ADC1, USB_MONITOR_CHANNEL, 2, ADC_SampleTime_4Cycles);
//     ADC_RegularChannelConfig(ADC1, JOYSTICK_Y_CHANNEL, 3, ADC_SampleTime_11Cycles);
//     ADC_RegularChannelConfig(ADC1, JOYSTICK_X_CHANNEL, 4, ADC_SampleTime_11Cycles);
//     ADC_RegularChannelConfig(ADC1, AIN0_CHANNEL, 5, ADC_SampleTime_11Cycles);
//     ADC_RegularChannelConfig(ADC1, AIN1_CHANNEL, 6, ADC_SampleTime_11Cycles);
//     ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 7, ADC_SampleTime_11Cycles);

//     // ADC_AnalogWatchdogThresholdsConfig(ADC1, );
//     ADC_AnalogWatchdogCmd(ADC1, ADC_AnalogWatchdog_AllRegEnable);

//     ADC_DMACmd(ADC1, ENABLE);
//     ADC_Cmd(ADC1, ENABLE);

//     // TODO: set thresholds and enable the analog watchdog interrupt to use the joysticks as buttons
//     ADC_ITConfig(ADC1, ADC_IT_AWD, ENABLE);
//     NVIC_EnableIRQ(ADC1_IRQn);

//     // ADC_ResetCalibration(ADC1);
//     // while (ADC_GetResetCalibrationStatus(ADC1))
//     //     ;
//     // ADC_StartCalibration(ADC1);
//     // while (ADC_GetCalibrationStatus(ADC1))
//     //     ;
// }

// static void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, uint32_t peripheralAddress, uint32_t memoryAddress, uint16_t bufferSize)
// {
//     DMA_InitTypeDef DMA_InitStructure = {0};

//     RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

//     DMA_DeInit(DMA_CHx);
//     DMA_InitStructure.DMA_PeripheralBaseAddr = peripheralAddress;
//     DMA_InitStructure.DMA_MemoryBaseAddr = memoryAddress;
//     DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
//     DMA_InitStructure.DMA_BufferSize = bufferSize;
//     DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
//     DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
//     DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
//     DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
//     DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
//     DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
//     DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
//     DMA_Init(DMA_CHx, &DMA_InitStructure);
// }

// static void ADC_Function_Init(void)
// {
//     ADC_InitTypeDef ADC_InitStructure = {0};
//     GPIO_InitTypeDef GPIO_InitStructure = {0};

//     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
//     RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

//     // Determines clock for ADC.
//     // At PCLK2 = 48MHz, ADCCLOCK = PCLK2 / 8 = 6MHz
//     // RCC_ADCCLKConfig(RCC_PCLK2_Div8);

//     GPIO_InitStructure.GPIO_Pin = BATTERY_MONITOR_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//     GPIO_Init(BATTERY_MONITOR_PORT, &GPIO_InitStructure);

//     GPIO_InitStructure.GPIO_Pin = USB_MONITOR_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//     GPIO_Init(USB_MONITOR_PORT, &GPIO_InitStructure);

//     GPIO_InitStructure.GPIO_Pin = JOYSTICK_Y_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//     GPIO_Init(JOYSTICK_Y_PORT, &GPIO_InitStructure);

//     GPIO_InitStructure.GPIO_Pin = JOYSTICK_X_PIN;
//     GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
//     GPIO_Init(JOYSTICK_X_PORT, &GPIO_InitStructure);

//     ADC_DeInit(ADC1);
//     ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
//     ADC_InitStructure.ADC_ScanConvMode = DISABLE;
//     ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
//     ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
//     ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
//     ADC_InitStructure.ADC_NbrOfChannel = 1;
//     ADC_Init(ADC1, &ADC_InitStructure);

//     ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
//     ADC_Cmd(ADC1, ENABLE);

//     // ADC_BufferCmd(ADC1, ENABLE); // enable buffer
// }

// static u16 Get_ADC_Val(u8 ch)
// {
//     u16 val = 0;
//     ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_11Cycles); // or 7?
//     ADC_SoftwareStartConvCmd(ADC1, ENABLE);
//     while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
//         ;
//     val = ADC_GetConversionValue(ADC1);
//     return val;
// }

// static u16 Get_ADC_Average(u8 ch, u8 times)
// {
//     u32 temp_val = 0;
//     u8 t;
//     u16 val;

//     for (t = 0; t < times; t++)
//     {
//         temp_val += Get_ADC_Val(ch);
//         Delay_Ms(5);
//     }

//     val = temp_val / times;

//     return val;
// }

/* main */
int main(void)
{
    // u16 ADCBuffer[ADC_CHANNELS];

#ifdef NVIC_PriorityGroup_2
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
#else
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
#endif
    SystemCoreClockUpdate();
    Delay_Init();
    USART4_Printf_Init(115200);

    /* makes sure that we can still flash using SWD */
    Delay_Ms(1000); // give serial monitor time to open

    PRINT("SystemClk: %u\r\n", (unsigned)SystemCoreClock);
    PRINT("ChipID: %08x\r\n", (unsigned)DBGMCU_GetCHIPID());

    IIC_Init(); // maps SWD lines to I2C
    Gpio_Init();
    // PWM_Init();
    // ADC_Function_Init();
    // ADC_MultiChannel_Init();
    // DMA_Tx_Init(DMA1_Channel1, (u32)&ADC1->RDATAR, (u32)ADCBuffer, ADC_CHANNELS);
    // DMA_Cmd(DMA1_Channel1, ENABLE);
    // ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* enable AUX power */
    GPIO_WriteBit(AUX_POWER_PORT, AUX_POWER_PIN, Bit_SET);

    /* Set LCD reset pin */
    GPIO_WriteBit(LCD_RESET_PORT, LCD_RESET_PIN, Bit_RESET);

    /* Turn on the LCD backlight */
    GPIO_WriteBit(LCD_BACKLIGHT_PORT, LCD_BACKLIGHT_PIN, Bit_SET);

    /* set the interrupt output pin */
    GPIO_WriteBit(INT_OUTPUT_PORT, INT_OUTPUT_PIN, Bit_SET);

    PRINT("Expander Init done\r\n");

    /* Set debug LED value */

    // uint8_t ledState = 0;
    while (1)
    {
        // __WFI(); // Wait for Interrupt (low power mode)
        GPIO_WriteBit(DEBUG_LED_PORT, DEBUG_LED_PIN, Bit_SET);
        Delay_Ms(1000);
        GPIO_WriteBit(DEBUG_LED_PORT, DEBUG_LED_PIN, Bit_RESET);
        Delay_Ms(1000);

        // Delay_Ms(100);
        // for (int i = 0; i < ADC_CHANNELS; i++)
        // {
        //     float adcVoltage = calculateVoltage(ADCBuffer[i]);
        //     int wholePart = (int)adcVoltage;
        //     int decimalPart = (int)((adcVoltage - wholePart) * 10000);
        //     PRINT("Channel - %d : %d.%04d ", i + 1, wholePart, decimalPart);
        // }
        // // ADC_val = Get_ADC_Average(ADC_CHANNEL_TO_USE, 10);
        // // PRINT("ADC val: %d\r\n", ADC_val);
        // // GPIO_WriteBit(DEBUG_LED_GPIO_PORT, DEBUG_LED_GPIO_PIN, ledState);
        // ledState ^= 1; // invert for the next run
        // Delay_Ms(1000);
    }
}

/* interrupt handlers */

// void ADC1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
// void ADC1_IRQHandler(void)
// {
//     if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET)
//     {
//         // PRINT("Analog watchdog triggered\r\n");
//         ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
//     }
// }

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void NMI_Handler(void)
{
    PRINT("NMI_Handler\r\n");
}

void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void)
{
    PRINT("HARDFAULT\r\n");
    while (1)
    {
    }
}

// void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
// void EXTI7_0_IRQHandler(void)
// {
//     if (EXTI_GetITStatus(EXTI_Line0) != RESET)
//     {
//         PRINT("interrupt\r\n");
//         inputChanged = 1; // Setting a flag instead of printing directly from the ISR()
//         // read all inputs?
//         EXTI_ClearITPendingBit(EXTI_Line0); // Clearing ISR flag
//     }
// }

static void I2C1_ClearErrorFlags(void)
{ // clear the various error flags that may block further communication

    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) != RESET)
    { //  I2C_FLAG_AF - Acknowledge failure flag.
        I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    }
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BERR) != RESET)
    { //  I2C_FLAG_BERR -Bus Error flag.
        I2C_ClearFlag(I2C1, I2C_FLAG_BERR);
    }
}

static void I2C1_ClearStopFlag(void)
{ // clear the stop flag
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF) != RESET)
    { // Stop detection flag (Slave mode).
        // STOPF (STOP detection) is cleared by software sequence: a read operation
        // to I2C_STAR1 register (I2C_GetFlagStatus()) followed by a write operation
        // to I2C_CTLR1 register (I2C_Cmd() to re-enable the I2C peripheral).
        // -> Since we just read the flag, we only need to (re-)enable.
        I2C_Cmd(I2C1, ENABLE);
    }
}

/**
 * @brief  Read bytes from master using a timeout
 * @param  data: pointer to data to be read
 * @param  size: number of bytes to be write.
 * @retval status
 */
static int i2c_slave_read(uint8_t *data, uint16_t size)
{
    uint8_t i = 0;
    int ret = 0;
    uint32_t tickstart = SysTick->CNT;

    while (i < size)
    {
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
        {
            data[i++] = I2C_ReceiveData(I2C1);
        }
        if ((SysTick->CNT - tickstart) >= I2C_TIMEOUT_TICK)
        {
            ret = I2C_TIMEOUT;
            break;
        }
    }
    return ret;
}

// MMOLE: added function to process I2C slave data transfers
static void i2c_slave_process(void)
{ // Process incoming and outgoing I2C data.
    // When processing the data we can assume there is an address match.
    // We could wait for an address match, but that would be blocking
    // and isn't needed as RX/TX-flags are only set when addressed properly.

    // if (I2C_CheckEvent(I2C1, I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED)) /* TRA, BUSY, TXE and ADDR flags */
    // {
    //     PRINT("I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED\r\n");
    // }

    // if (I2C_CheckEvent(I2C1, I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED)) /* BUSY and ADDR flags */
    // {
    //     PRINT("I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED\r\n");
    // }

    // if (I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) != RESET)
    // {
    //     PRINT("I2C_FLAG_ADDR\r\n");
    // }

    // if (I2C_CheckEvent(I2C1, I2C_EVENT_SLAVE_BYTE_RECEIVED)) /* BUSY and RXNE flags */
    // {
    //     PRINT("I2C_EVENT_SLAVE_BYTE_RECEIVED\r\n");
    // }

    // Process receiving data
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
    { // Data register not empty (Receiver) flag; read all available data and store it
        I2C_REG_ptr = I2C_ReceiveData(I2C1);
        PRINT("address %x\r\n", I2C_REG_ptr);
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
        {
            char c = I2C_ReceiveData(I2C1);
            PRINT("received %x\r\n", c);
            test_buffer[I2C_REG_ptr++] = c;
        }
    }

    // Process end of receiving data, as determined by stop flag
    if (I2C_CheckEvent(I2C1, I2C_EVENT_SLAVE_STOP_DETECTED))
    { // When done receiving let's do the callback
        // Note: twi.c::i2c_onSlaveReceive is tied to the TwoWire::onReceiveService().
        PRINT("all data received\r\n");

        // clear the stop flag to be ready for another session
        I2C1_ClearStopFlag();
    }

    // Process transmitting data
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) != RESET)
    { // Data register empty flag (Transmitter).
        // It seems we need to send something, give the callback opportunity to do so.
        // Note: twi.c::i2c_onSlaveTransmit is tied to the TwoWire::onRequestService().
        // The onRequest callback uses Wire.write(), which calls twi.c::i2c_slave_write(),
        // which then calls I2C_SendData() and checks if done within timeout */
        PRINT("sending\r\n");
        I2C_SendData(I2C1, test_buffer[I2C_REG_ptr++]); // send register value to master
    }

    // just for debugging
    if (I2C_CheckEvent(I2C1, I2C_EVENT_SLAVE_BYTE_TRANSMITTED))
    {
        PRINT("Master acked received byte (I2C_EVENT_SLAVE_BYTE_TRANSMITTED)\r\n");
    }

    if (I2C_CheckEvent(I2C1, I2C_EVENT_SLAVE_ACK_FAILURE))
    {
        PRINT("Master stopped receiving (I2C_EVENT_SLAVE_ACK_FAILURE)\r\n");
    }

    // Clear error flags (since we don't handle them anyways)
    I2C1_ClearErrorFlags();
}

void I2C1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void I2C1_IRQHandler(void)
{
    PRINT("I2C1_IRQHandler\r\n");
}

// Interrupt Service Routine for I2C1 Event
void I2C1_EV_IRQHandler(void) __attribute__((interrupt));
void I2C1_EV_IRQHandler(void)
{
    // see: https://github.com/cnlohr/ch32fun/blob/master/examples_x035/i2c_slave_test/i2c_slave_test.c
    // see: https://github.com/Community-PIO-CH32V/ch32v003fun/blob/master/examples/i2c_slave/i2c_slave.h
    // see: https://github.com/maxint-rd/arduino_core_ch32/blob/main/libraries/Wire/src/utility/twi.c

    i2c_slave_process();
}

// Interrupt Service Routine for I2C1 Error
void I2C1_ER_IRQHandler(void) __attribute__((interrupt));
void I2C1_ER_IRQHandler(void)
{
    // do nothing here
}
