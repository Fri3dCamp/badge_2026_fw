#include <ch32x035.h> /* both X033 and X035 */
#include <stdlib.h>   /* atoi() */
#include <string.h>   /* memset() */

#include "debug.h"

/* analog inputs */
#define AIN1_PORT               GPIOA // PA0: Ain1
#define AIN1_PIN                GPIO_Pin_0
#define AIN1_CHANNEL            ADC_Channel_0
#define AIN1_RANK               (1)
#define AIN0_PORT               GPIOA // PA1: Ain0
#define AIN0_PIN                GPIO_Pin_1
#define AIN0_CHANNEL            ADC_Channel_1
#define AIN0_RANK               (2)
#define BATTERY_MONITOR_PORT    GPIOC // PC0: Battery Monitor
#define BATTERY_MONITOR_PIN     GPIO_Pin_0
#define BATTERY_MONITOR_CHANNEL ADC_Channel_9
#define BATTERY_MONITOR_RANK    (3)
#define USB_MONITOR_PORT        GPIOC // PC3: USB Monitor
#define USB_MONITOR_PIN         GPIO_Pin_0
#define USB_MONITOR_CHANNEL     ADC_Channel_13 // TODO: or 10?
#define USB_MONITOR_RANK        (4)
#define JOYSTICK_Y_PORT         GPIOA // PA6: JoyY
#define JOYSTICK_Y_PIN          GPIO_Pin_6
#define JOYSTICK_Y_CHANNEL      ADC_Channel_6
#define JOYSTICK_Y_RANK         (5)
#define JOYSTICK_X_PORT         GPIOA // PA5: JoyX
#define JOYSTICK_X_PIN          GPIO_Pin_5
#define JOYSTICK_X_CHANNEL      ADC_Channel_5
#define JOYSTICK_X_RANK         (6)
#define ADC_CHANNELS            (6)

/* digital inputs */
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

/* digital outputs */
#define AUX_POWER_PORT  GPIOB // PB6: Aux power
#define AUX_POWER_PIN   GPIO_Pin_6
#define LCD_RESET_PORT  GPIOB // PB11: LCD reset
#define LCD_RESET_PIN   GPIO_Pin_11
#define INT_OUTPUT_PORT GPIOC // PC16: Interrupt output
#define INT_OUTPUT_PIN  GPIO_Pin_16
#define DEBUG_LED_PORT  GPIOA // PA4: debug LED
#define DEBUG_LED_PIN   GPIO_Pin_4

// PWM
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

/* optional feature: analog watchdog limits */
#define JOYSTICK_THRESHOLD_TOP    (3000)
#define JOYSTICK_THRESHOLD_BOTTOM (1000)
#define USB_VOLTAGE_THRESHOLD     (3000) /* (5V/2 / 3.3) * 4095 = 3100, we consider everything above 3000 as "USB connected" */

// TODO: is this pulse width enough?
#define INT_PULSE_TICKS (SystemCoreClock-1) /* one second*/

#define RESULT_BUFFER_SIZE (3 + 2 + (ADC_CHANNELS * 2) + 1 + 1 + 1 + 1)
#define OUTPUTS_OFFSET     (3 + 2 + (ADC_CHANNELS * 2))
#define PWM_LCD_OFFSET     (OUTPUTS_OFFSET + 1)
#define PWM_BUZZER_OFFSET  (PWM_LCD_OFFSET + 1)

#define BUTTON_SIZE (2)

typedef struct
{
    uint8_t charger_charging : 1; /* charger is charging */
    uint8_t charger_standby : 1;  /* charger is standby */
    uint8_t button_x : 1;         /* button x state */
    uint8_t button_y : 1;         /* button y state */
    uint8_t button_a : 1;         /* button a state */
    uint8_t button_b : 1;         /* button b state */
    uint8_t button_menu : 1;      /* button menu state */
    uint8_t joy_up : 1;           /* joystick up */
    uint8_t joy_down : 1;         /* joystick down */
    uint8_t joy_left : 1;         /* joystick up */
    uint8_t joy_right : 1;        /* joystick down */
    uint8_t usb_plugged : 1;      /* USB is plugged in */
    uint8_t reserved : 4;
} buttons_t;

/*
 * This struct contains all data that is available through I2C.
 */
typedef struct
{
    uint8_t version[3];
    buttons_t inputs;
    uint16_t adc_channels[ADC_CHANNELS]; /* current value for all ADC channels */
    uint8_t aux_power : 1;               /* set aux power on/off */
    uint8_t lcd_reset : 1;               /* reset LCD */
    uint8_t output_reserved : 6;         /**/
    uint8_t lcd_brightness;              /* LCD PWM brightness */
    uint8_t buzzer;                      /* buzzer PWM value */
} addon_data_t;

typedef struct
{
    uint8_t flag_update_lcd : 1;       // flag to indicate that the LCD PWM value should be updated
    uint8_t flag_update_buzzer : 1;    // flag to indicate that the buzzer PWM value should be updated
    uint8_t flag_update_outputs : 1;   // flag to indicate that the outputs should be updated
    uint8_t flag_button_scan_done : 1; // flag to indicate that the state of one of the buttons has changed
    uint8_t flag_clear_int : 1;         // flag to indicate that the interrupt towards the ESP32 can be cleared
    uint8_t reserved : 3;              // reserved for future use
    uint8_t raw_data_ptr;              // current index in the raw_data buffer to read/write using I2C
    union
    {
        addon_data_t data;
        uint8_t raw_data[RESULT_BUFFER_SIZE];
    };
} addon_state_t;

static addon_state_t state;

static void SysTick_Init(void)
{
    SysTick->SR = 0;
    SysTick->CNT = 0;
    SysTick->CMP = INT_PULSE_TICKS;
    SysTick->CTLR = 0xF;

    NVIC_SetPriority(SysTick_IRQn, 15);
    NVIC_EnableIRQ(SysTick_IRQn);
}

static void Outputs_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};

    // outputs
    GPIO_InitStructure.GPIO_Pin = AUX_POWER_PIN | LCD_RESET_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = INT_OUTPUT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(INT_OUTPUT_PORT, &GPIO_InitStructure);

    // No more timers left to make this a PWM
    GPIO_InitStructure.GPIO_Pin = DEBUG_LED_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DEBUG_LED_PORT, &GPIO_InitStructure);
}

/*********************************************************************
 * @fn      Button_Init
 *
 * @brief   Initialize timer3 for button scan
 *
 * @param   arr - The specific period value
 *          psc - The specifies prescaler value
 *
 * @return  none
 */
static void Button_Init(uint16_t arr, uint16_t psc)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    /* Enable Timer3 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    /* Enable AFIO, GPIO A, B and C clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    /* configure GPIO as inputs */
    GPIO_InitStructure.GPIO_Pin = CHARGER_CHARGING_PIN | CHARGER_STANDBY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = BUTTON_X_PIN | BUTTON_A_PIN | BUTTON_B_PIN | BUTTON_Y_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = BUTTON_MENU_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUTTON_MENU_PORT, &GPIO_InitStructure);

    /* Initialize Timer3 */
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    /* enable timer interrupts */
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    /* configure timer interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* Enable Timer3 */
    TIM_Cmd(TIM3, ENABLE);
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

static void PWM_Init(TIM_TypeDef *TIMx, uint16_t PRSC, uint16_t ARR, uint16_t CCR)
{
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};

    TIM_Cmd(TIMx, DISABLE);                                         // Disable timer before (re)configuring it
    TIM_TimeBaseInitStructure.TIM_Period = ARR;                     // Auto-Reload Register (counter's max value before overflowing)
    TIM_TimeBaseInitStructure.TIM_Prescaler = PRSC;                 // Prescaler - Main clock's divider
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     // No clock division
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; // Counting upwards
    TIM_TimeBaseInit(TIMx, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             // PWM mode
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; // PWM is also directed to a physical pin
    TIM_OCInitStructure.TIM_Pulse = CCR;                          // Capture-compare register (PWM changes the OCPolarity when counter reaches this value)
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     // PWM is high when the counter starts at 0
    TIM_OC1Init(TIMx, &TIM_OCInitStructure);

    /* enable TIMx */
    TIM_CtrlPWMOutputs(TIMx, ENABLE);
    TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIMx, ENABLE);
    TIM_Cmd(TIMx, ENABLE);
}

static void LCD_PWM_Init(uint16_t PRSC, uint16_t ARR, uint16_t CCR)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* enable timers and GPIO clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOB, ENABLE);

    /* set pinmux */
    GPIO_PinRemapConfig(GPIO_PartialRemap2_TIM1, ENABLE); // set LCD backlight (PB12) on CH4 of TIM1

    /* configure GPIO as PWM output */
    GPIO_InitStructure.GPIO_Pin = LCD_BACKLIGHT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_BACKLIGHT_PORT, &GPIO_InitStructure);

    /* configure TIM1 : LCD backlight */
    PWM_Init(LCD_BACKLIGHT_TIM, PRSC, ARR, CCR);
}

static void Buzzer_PWM_Init(uint16_t PRSC, uint16_t ARR, uint16_t CCR)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    /* enable timers and GPIO clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* set pinmux */
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, ENABLE); // set Buzzer (PC14) on CH2 of TIM2

    /* configure GPIO as PWM output */
    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);

    /* configure TIM2 : Buzzer */
    PWM_Init(BUZZER_TIM, PRSC, ARR, CCR);
}

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

/* initialize multicahnnel ADC reading
 * reference: https://curiousscientist.tech/blog/ch32v003f4p6-adc-basics
 */
static void ADC_MultiChannel_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    // NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = JOYSTICK_X_PIN | JOYSTICK_Y_PIN | AIN0_PIN | AIN1_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = BATTERY_MONITOR_PIN | USB_MONITOR_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // turn off ADC before we configure it
    ADC_DeInit(ADC1);

    /* dial down the clock so that we have stable readings */
    ADC_CLKConfig(ADC1, ADC_CLK_Div16);

    // configure the ADC
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;                  // operate in independent mode
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;                        // scan multiple channels
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;                  // continuous conversion
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; // no external trigger to start the conversion of regular channels
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;              // right align the ADC data
    ADC_InitStructure.ADC_NbrOfChannel = ADC_CHANNELS;                  // number of regular ADC channels to convert
    ADC_InitStructure.ADC_OutputBuffer = 0;                             // Not used on CH32X035? set to 0
    ADC_InitStructure.ADC_Pga = 0;                                      // Not used on CH32X035? set to 0
    ADC_Init(ADC1, &ADC_InitStructure);

    // configure the ADC channels
    ADC_RegularChannelConfig(ADC1, BATTERY_MONITOR_CHANNEL, BATTERY_MONITOR_RANK, ADC_SampleTime_11Cycles);
    ADC_RegularChannelConfig(ADC1, USB_MONITOR_CHANNEL, USB_MONITOR_RANK, ADC_SampleTime_11Cycles);
    ADC_RegularChannelConfig(ADC1, JOYSTICK_Y_CHANNEL, JOYSTICK_Y_RANK, ADC_SampleTime_11Cycles);
    ADC_RegularChannelConfig(ADC1, JOYSTICK_X_CHANNEL, JOYSTICK_X_RANK, ADC_SampleTime_11Cycles);
    ADC_RegularChannelConfig(ADC1, AIN0_CHANNEL, AIN0_RANK, ADC_SampleTime_11Cycles);
    ADC_RegularChannelConfig(ADC1, AIN1_CHANNEL, AIN1_RANK, ADC_SampleTime_11Cycles);

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);
}

static void DMA_Tx_Init(DMA_Channel_TypeDef *DMA_CHx, uint32_t peripheralAddress, uint32_t memoryAddress, uint16_t bufferSize)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);
    DMA_InitStructure.DMA_PeripheralBaseAddr = peripheralAddress;
    DMA_InitStructure.DMA_MemoryBaseAddr = memoryAddress;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = bufferSize;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);
}

/* clear the various error flags that may block further communication */
static void I2C1_ClearErrorFlags(void)
{

    /* I2C_FLAG_AF - Acknowledge failure flag */
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) != RESET)
    {
        I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    }
    /* I2C_FLAG_BERR -Bus Error flag.*/
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BERR) != RESET)
    {
        I2C_ClearFlag(I2C1, I2C_FLAG_BERR);
    }
}

/* clear the stop flag */
static void I2C1_ClearStopFlag(void)
{
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_STOPF) != RESET)
    {
        /* Stop detection flag (Slave mode).
         * STOPF (STOP detection) is cleared by software sequence: a read operation
         * to I2C_STAR1 register (I2C_GetFlagStatus()) followed by a write operation
         * to I2C_CTLR1 register (I2C_Cmd() to re-enable the I2C peripheral).
         * -> Since we just read the flag, we only need to (re-)enable.
         * */
        I2C_Cmd(I2C1, ENABLE);
    }
}



/* function to process I2C slave data transfers */
/* reference: arduino implementation */
static void i2c_slave_process(void)
{
    /* Process incoming and outgoing I2C data.
     * When processing the data we can assume there is an address match.
     * We could wait for an address match, but that would be blocking
     * and isn't needed as RX/TX-flags are only set when addressed properly.
     */

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

    /* Process receiving data */
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
    {
        /* Data register not empty (Receiver) flag
         * read all available data and store it
         */
        state.raw_data_ptr = I2C_ReceiveData(I2C1);
        PRINT("address %x\r\n", state.raw_data_ptr);
        // TODO: use i2c_slave_read() here?
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
        {
            char c = I2C_ReceiveData(I2C1);
            PRINT("received %x\r\n", c);
            if (state.raw_data_ptr < RESULT_BUFFER_SIZE)
            {
                // TODO: implement this
                switch (state.raw_data_ptr)
                {
                    case PWM_LCD_OFFSET:
                        state.raw_data[state.raw_data_ptr++] = c;
                        state.flag_update_lcd = 1;
                        break;
                    case PWM_BUZZER_OFFSET:
                        state.raw_data[state.raw_data_ptr++] = c;
                        state.flag_update_buzzer = 1;
                        break;
                    case OUTPUTS_OFFSET:
                        state.raw_data[state.raw_data_ptr++] = c;
                        state.flag_update_outputs = 1;
                        break;
                    default:
                        PRINT("Trying to write in readonly memory");
                }
            }
            else
            {
                // TODO: do a reboot here and trigger bootloader?
                PRINT("ERROR: trying to write 0x%x outside of result buffer: 0x%x\r\n", c, state.raw_data_ptr);
            }
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

    /* Process transmitting data */
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_TXE) != RESET)
    {
        /* Data register empty flag (Transmitter).
         * It seems we need to send something
         */
        if (state.raw_data_ptr < RESULT_BUFFER_SIZE)
        {
            PRINT("sending\r\n");
            I2C_SendData(I2C1, state.raw_data[state.raw_data_ptr++]); // send register value to master
        }
        else
        {
            PRINT("ERROR: reading dummy data\r\n");
            I2C_SendData(I2C1, 0x00); // send dummy data to master
        }
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

    /* Clear error flags (since we don't handle them anyways) */
    I2C1_ClearErrorFlags();
}

static buttons_t read_buttons(void)
{
    buttons_t res = {0};
    uint32_t a = GPIO_ReadInputData(GPIOA);
    uint32_t b = GPIO_ReadInputData(GPIOB);
    uint32_t c = GPIO_ReadInputData(GPIOC);
    /* take a local copy of the current ADC values */
    uint16_t joy_x = state.data.adc_channels[JOYSTICK_X_RANK - 1];
    uint16_t joy_y = state.data.adc_channels[JOYSTICK_Y_RANK - 1];
    uint16_t usb_voltage = state.data.adc_channels[USB_MONITOR_RANK - 1];

    // TODO: check polarities

    if ((a & CHARGER_CHARGING_PIN) == (uint32_t)Bit_RESET)
    {
        res.charger_charging = 1;
    }

    if ((a & CHARGER_STANDBY_PIN) == (uint32_t)Bit_RESET)
    {
        res.charger_standby = 1;
    }

    if ((b & BUTTON_X_PIN) == (uint32_t)Bit_RESET)
    {
        res.button_x = 1;
    }

    if ((b & BUTTON_A_PIN) == (uint32_t)Bit_RESET)
    {
        res.button_a = 1;
    }

    if ((b & BUTTON_B_PIN) == (uint32_t)Bit_RESET)
    {
        res.button_b = 1;
    }

    if ((b & BUTTON_Y_PIN) == (uint32_t)Bit_RESET)
    {
        res.button_y = 1;
    }

    if ((c & BUTTON_MENU_PIN) == (uint32_t)Bit_RESET)
    {
        res.button_menu = 1;
    }

    // if (state.flag_check_bounds) // TODO: is this needed?
    // {
    //     state.flag_check_bounds = 0;
    if (joy_y > JOYSTICK_THRESHOLD_TOP)
    {
        res.joy_up = 1;
    }
    if (joy_y < JOYSTICK_THRESHOLD_BOTTOM)
    {
        res.joy_down = 1;
    }
    if (joy_x > JOYSTICK_THRESHOLD_TOP)
    {
        res.joy_right = 1;
    }
    if (joy_x < JOYSTICK_THRESHOLD_BOTTOM)
    {
        res.joy_left = 1;
    }
    if (usb_voltage > USB_VOLTAGE_THRESHOLD)
    {
        res.usb_plugged = 1;
    }
    // }

    return res;
}

/*********************************************************************
 * @fn      Button_Scan
 *
 * @brief   Perform input button scan.
 *
 * @return  none
 */
static void Button_Scan(void)
{
    static uint8_t scan_cnt = 0;
    static buttons_t previous_button_state = {0};

    scan_cnt++;
    if ((scan_cnt % 10) == 0)
    {
        /* reset the debounce counter */
        scan_cnt = 0;

        /* Determine whether the two scan results are consistent (debouncing) */
        buttons_t button_state = read_buttons();
        if (memcmp(&button_state, &previous_button_state, BUTTON_SIZE) == 0 && memcmp(&button_state, &state.data.inputs, BUTTON_SIZE) != 0)
        {
            /* now set the result for this column scan */
            state.flag_button_scan_done = 1;
            state.data.inputs = button_state;
            memset(&previous_button_state, 0, BUTTON_SIZE);
        }
    }
    else if ((scan_cnt % 5) == 0)
    {
        /* Save the first scan result */
        previous_button_state = read_buttons();
    }
}

static void set_int_output(BitAction BitVal)
{
    GPIO_WriteBit(DEBUG_LED_PORT, DEBUG_LED_PIN, BitVal == Bit_RESET ? Bit_SET : Bit_RESET);
    GPIO_WriteBit(INT_OUTPUT_PORT, INT_OUTPUT_PIN, BitVal);
    if (BitVal != Bit_RESET)
    {
        SysTick->CNT = 0;
        state.flag_clear_int = 0;
    }
}

static void reset_to_bootloader(void) {
    SystemReset_StartMode(Start_Mode_BOOT);
    NVIC_SystemReset();
}

/* main */
int main(void)
{
    /* set all data and flags to 0 */
    memset(&state, 0, sizeof(addon_state_t));

    /* set the version number from git */
    state.data.version[0] = atoi(VERSION_MAJOR) & 0xff;
    state.data.version[1] = atoi(VERSION_MINOR) & 0xff;
    state.data.version[2] = atoi(VERSION_PATCH) & 0xff;

    SystemInit();
#ifdef NVIC_PriorityGroup_2
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
#else
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
#endif
    SystemCoreClockUpdate();
    Delay_Init();
#if (DEBUG)
    USART4_Printf_Init(115200);
#endif

    /* makes sure that we can still flash using SWD */
    Delay_Ms(1000); // give serial monitor time to open

    PRINT("SystemClk: %u\r\n", (unsigned)SystemCoreClock);
    PRINT("ChipID: %08x\r\n", (unsigned)DBGMCU_GetCHIPID());

    /* configure the I2C pins and interrupts */
    IIC_Init(); // maps SWD lines to I2C
    /* configure the GPIO in- and outputs */
    Outputs_Init();

    /* configure the button debounce timer */
    Button_Init(1, SystemCoreClock / 10000 - 1);
    Buzzer_PWM_Init(480 - 1, 100, 75);
    LCD_PWM_Init(480 - 1, 100, 75);
    /* configure the analog input reading */
    ADC_MultiChannel_Init();
    DMA_Tx_Init(DMA1_Channel1, (u32)&ADC1->RDATAR, (u32)state.data.adc_channels, ADC_CHANNELS);
    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    SysTick_Init();

    /* enable AUX power and unset the LCD reset pin */
    state.data.aux_power = 1;
    state.data.lcd_reset = 0;
    state.flag_update_outputs = 1;

    /* Turn on the LCD backlight */
    // TODO: set the PWM to 100% or 50%?
    state.data.lcd_brightness = 0xFF >> 1;
    state.flag_update_lcd = 1;

    /* turn on the buzzer at boot */
    state.data.buzzer = 0xff >> 1;
    state.flag_update_buzzer = 1;

    /* set the interrupt output pin */
    set_int_output(Bit_SET);

    PRINT("Expander Init done\r\n");

    while (1)
    {
        if (state.flag_button_scan_done)
        {
            state.flag_button_scan_done = 0;
            set_int_output(Bit_SET);
            if (state.data.inputs.button_menu) {
                PRINT("Menu button pressed, rebooting to bootloader in 10 seconds\r\n");
                Delay_Ms(10000);
                PRINT("rebooting\r\n");
                Delay_Ms(100);
                reset_to_bootloader();
                while(1);
            }
        }

        /* check if the interrupt output pin can be reset */
        if (state.flag_clear_int)
        {
            set_int_output(Bit_RESET);
        }

        if (state.flag_update_outputs)
        {
            state.flag_update_outputs = 0;
            GPIO_WriteBit(AUX_POWER_PORT, AUX_POWER_PIN, state.data.aux_power ? Bit_SET : Bit_RESET);
            GPIO_WriteBit(LCD_RESET_PORT, LCD_RESET_PIN, state.data.lcd_reset ? Bit_SET : Bit_RESET);
        }

        if (state.flag_update_lcd)
        {
            state.flag_update_lcd = 0;
            PWM_Init(LCD_BACKLIGHT_TIM, 480 - 1, 100, 75);
        }

        if (state.flag_update_buzzer)
        {
            state.flag_update_buzzer = 0;
            PWM_Init(BUZZER_TIM, 480 - 1, 100, 75);
        }
    }
}

/* interrupt handlers */

// void ADC1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
// void ADC1_IRQHandler(void)
// {
//     if (ADC_GetITStatus(ADC1, ADC_IT_AWD) != RESET)
//     {
//         check_bounds = 1;
//         ADC_ClearITPendingBit(ADC1, ADC_IT_AWD);
//     }
// }

void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        /* Clear interrupt flag */
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

        /* Handle button scan */
        Button_Scan();
    }
}

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

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void)
{
    SysTick->SR = 0;
    state.flag_clear_int = 1;
}
