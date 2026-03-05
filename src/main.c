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
#define BATTERY_MONITOR_CHANNEL ADC_Channel_10
#define BATTERY_MONITOR_RANK    (3)
#define USB_MONITOR_PORT        GPIOC // PC3: USB Monitor
#define USB_MONITOR_PIN         GPIO_Pin_3
#define USB_MONITOR_CHANNEL     ADC_Channel_13
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
#define ADC_DMA_CHANNEL         DMA1_Channel1

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
#define INT_OUTPUT_PORT GPIOC // PC17: Interrupt output
#define INT_OUTPUT_PIN  GPIO_Pin_17

// PWM
#define DEBUG_LED_PORT                GPIOA // PA4: debug LED
#define DEBUG_LED_PIN                 GPIO_Pin_4
#define DEBUG_LED_TIM                 TIM3                // TIM3 Channel 2
#define DEBUG_LED_TIM_REMAP           GPIO_FullRemap_TIM3 // mapping 0b11
#define DEBUG_LED_TIM_CVR             TIM3->CH2CVR        // TIM3 Channel 2 compare register
#define LCD_BACKLIGHT_PORT            GPIOB               // PB12: LCD Backlight
#define LCD_BACKLIGHT_PIN             GPIO_Pin_12
#define LCD_BACKLIGHT_TIM             TIM1                    // TIM1 Channel 4
#define LCD_BACKLIGHT_TIM_REMAP       GPIO_PartialRemap2_TIM1 // mapping 0b010
#define LCD_BACKLIGHT_TIM_CVR         TIM1->CH4CVR            // TIM1 Channel 4 compare register
#define LCD_BACKLIGHT_TIM_DMA_CHANNEL DMA1_Channel5           // DMA channel for TIM1_UP

// I2C
#define SDA_PORT         GPIOC
#define SDA_PIN          GPIO_Pin_18
#define SCL_PORT         GPIOC
#define SCL_PIN          GPIO_Pin_19
#define I2C_ADDRESS      (0x50)
#define I2C_TIMEOUT      (-2)
#define I2C_TIMEOUT_TICK ((SystemCoreClock / 10) - 1) /* 100 ms */

/* optional feature: analog watchdog limits */
#define JOYSTICK_THRESHOLD_TOP    (3000) /* Threshold used to convert a joystick ADC value to a digital signal */
#define JOYSTICK_THRESHOLD_BOTTOM (1000) /* Threshold used to convert a joystick ADC value to a digital signal */
#define USB_VOLTAGE_THRESHOLD     (3000) /* (5V/2 / 3.3) * 4095 = 3100, we consider everything above 3000 as "USB connected" */

#define TIMER_FREQ ((SystemCoreClock / 10000) - 1) /* the output frequency of all timers: 100Hz */

#define BUTTON_SIZE        (2)
#define RESULT_BUFFER_SIZE (3 + 1 + BUTTON_SIZE + (ADC_CHANNELS * 2) + 2 + 2 + 1)
#define PWM_LCD_OFFSET     (3 + 1 + BUTTON_SIZE + (ADC_CHANNELS * 2))
#define PWM_LED_OFFSET     (PWM_LCD_OFFSET + 2)
#define OUTPUTS_OFFSET     (PWM_LED_OFFSET + 2)

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
typedef struct __attribute__((packed))
{
    uint8_t version[3];                  /* version number */
    uint8_t unused;                      /* this byte is important to 4 byte align the ADC channels buffer */
    buttons_t inputs;                    /* buttons state */
    uint16_t adc_channels[ADC_CHANNELS]; /* current value for all ADC channels THIS LOCATION NEED TO BE 4 BYTE ALIGNED! */
    uint16_t lcd_brightness;             /* LCD PWM brightness */
    uint16_t led_brightness;             /* LED PWM brightness */
    uint8_t aux_power : 1;               /* set aux power on/off */
    uint8_t lcd_reset : 1;               /* reset LCD */
    uint8_t reboot : 1;                  /* reboot to bootloader */
    uint8_t output_reserved : 5;         /**/
} addon_data_t;

_Static_assert(sizeof(addon_data_t) == RESULT_BUFFER_SIZE, "raw data and struct size are not aligned!");

typedef struct
{
    uint8_t flag_update_led : 1;           // flag to indicate that the LED PWM value should be updated
    uint8_t flag_update_outputs : 1;       // flag to indicate that the outputs should be updated
    uint8_t flag_button_scan_halfway : 1;  // flag to indicate that scanning of the buttons has finished
    uint8_t flag_button_state_changed : 1; // flag to indicate that the state of one of the buttons has changed
    uint8_t reserved : 4;                  // reserved for future use
    uint8_t raw_data_ptr;                  // current index in the raw_data buffer to read/write using I2C
    union
    {
        addon_data_t data;
        uint8_t raw_data[RESULT_BUFFER_SIZE];
    };
} addon_state_t;

/* global state variable */
static addon_state_t state;

static void Outputs_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

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

    /* Enable Timer2 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    /* Enable AFIO, GPIO A, B and C clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    /* configure GPIO as inputs */
    GPIO_InitStructure.GPIO_Pin = CHARGER_CHARGING_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CHARGER_STANDBY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; // TODO: what should these be?
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

    /* Initialize Timer2 */
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* configure timer interrupt */
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_UP_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* enable timer interrupts */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* Enable Timer2 */
    TIM_Cmd(TIM2, ENABLE);
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

// reference: https://github.com/openwch/ch32x035/blob/main/EVT/EXAM/TIM/TIM_DMA/User/main.c
static void LCD_PWM_Init(uint16_t arr, uint16_t psc, uint16_t ccp)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    /* enable timers and GPIO clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOB, ENABLE);

    /* set pinmux */
    GPIO_PinRemapConfig(LCD_BACKLIGHT_TIM_REMAP, ENABLE); // set LCD backlight (PB12) on CH4 of TIM1

    GPIO_InitStructure.GPIO_Pin = LCD_BACKLIGHT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_BACKLIGHT_PORT, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(LCD_BACKLIGHT_TIM, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2; // High until CNT < CCR
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp; // start duty cycle
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(LCD_BACKLIGHT_TIM, &TIM_OCInitStructure);

    TIM_OC4PreloadConfig(LCD_BACKLIGHT_TIM, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(LCD_BACKLIGHT_TIM, ENABLE);
}

/*********************************************************************
 * @fn      TIM1_DMA_Init
 *
 * @brief   Initializes the TIM DMAy Channelx configuration.
 *
 * @param   DMA_CHx -
 *            x can be 1 to 7.
 *          ppadr - Peripheral base address.
 *          memadr - Memory base address.
 *          bufsize - DMA channel buffer size.
 *
 * @return  none
 */
static void LCD_PWM_DMA_Init(u32 memadr)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(LCD_BACKLIGHT_TIM_DMA_CHANNEL);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&LCD_BACKLIGHT_TIM_CVR;
    DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 1;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Disable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(LCD_BACKLIGHT_TIM_DMA_CHANNEL, &DMA_InitStructure);

    DMA_Cmd(LCD_BACKLIGHT_TIM_DMA_CHANNEL, ENABLE);
}

static void LED_PWM_Init(uint16_t arr, uint16_t psc, uint16_t ccp)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};

    /* enable timers and GPIO clocks */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE);

    /* set pinmux */
    GPIO_PinRemapConfig(DEBUG_LED_TIM_REMAP, ENABLE); // set LED (PA4) on CH2 of TIM3

    GPIO_InitStructure.GPIO_Pin = DEBUG_LED_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DEBUG_LED_PORT, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(DEBUG_LED_TIM, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2; // High until CNT < CCR
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init(DEBUG_LED_TIM, &TIM_OCInitStructure);

    TIM_OC2PreloadConfig(DEBUG_LED_TIM, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(DEBUG_LED_TIM, ENABLE);
}

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

static void reset_to_bootloader(void)
{
    SystemReset_StartMode(Start_Mode_BOOT);
    NVIC_SystemReset();
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
    uint32_t tickstart = SysTick->CNT;

    while (i < size && I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
    {
        data[i++] = I2C_ReceiveData(I2C1);
        if ((SysTick->CNT - tickstart) >= I2C_TIMEOUT_TICK)
        {
            PRINT("Read timeout\r\n");
            // ret = I2C_TIMEOUT;
            // break;
        }
    }
    return i;
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

    /* Process receiving data */
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
    {
        /* Data register not empty (Receiver) flag
         * read all available data and store it
         */
        state.raw_data_ptr = I2C_ReceiveData(I2C1);
        PRINT("address 0x%02x\r\n", state.raw_data_ptr);
        switch (state.raw_data_ptr)
        {
            case PWM_LCD_OFFSET: {
                uint16_t new_value;
                int ret = i2c_slave_read((uint8_t *)(&new_value), 2);
                state.raw_data_ptr += ret;
                if (ret == 2)
                {
                    state.data.lcd_brightness = new_value; // DMA will set the new value automatically
                }
                break;
            }
            case PWM_LED_OFFSET: {
                uint16_t new_value;
                int ret = i2c_slave_read((uint8_t *)(&new_value), 2);
                state.raw_data_ptr += ret;
                if (ret == 2)
                {
                    state.data.led_brightness = new_value;
                    // there is no DMA for TIM3, so we nuse this flag to set the new value manually
                    state.flag_update_led = 1;
                }
                break;
            }
            case OUTPUTS_OFFSET: {
                uint8_t new_value;
                int ret = i2c_slave_read((uint8_t *)(&new_value), 1);
                state.raw_data_ptr += ret;
                if (ret == 1)
                {
                    state.raw_data[OUTPUTS_OFFSET] = new_value;
                    state.flag_update_outputs = 1; // set the flag to update the outputs
                }
                break;
            }
            default: {
                while (I2C_GetFlagStatus(I2C1, I2C_FLAG_RXNE) != RESET)
                {
#if (DEBUG)
                    PRINT("received %x\r\n", I2C_ReceiveData(I2C1));
#else
                    I2C_ReceiveData(I2C1);
#endif
                }
                PRINT("we do not allow writing to offset 0x%02x\r\n", state.raw_data_ptr);
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

    return res;
}

/*********************************************************************
 * @fn      Button_Scan
 *
 * @brief   Perform input button scan. triggered every 10ms by a timer
 *
 * @return  none
 */
static void Button_Scan(void)
{
    static uint8_t scan_cnt = 0;
    static buttons_t previous_button_state = {0};

    scan_cnt++;
    if ((scan_cnt % 10) == 0) // every 100ms
    {
        /* reset the debounce counter */
        scan_cnt = 0;

        /* Determine whether the two scan results are consistent (debouncing) */
        buttons_t button_state = read_buttons();
        if (memcmp(&button_state, &previous_button_state, BUTTON_SIZE) == 0 && memcmp(&button_state, &state.data.inputs, BUTTON_SIZE) != 0)
        {
            /* now set the result for this column scan */
            state.flag_button_state_changed = 1;
            state.data.inputs = button_state;
            memset(&previous_button_state, 0, BUTTON_SIZE);
        }
    }
    else if ((scan_cnt % 5) == 0) // every 50ms
    {
        state.flag_button_scan_halfway = 1;
        /* Save the first scan result */
        previous_button_state = read_buttons();
    }
}

/* main */
int main(void)
{
    /* set all data and flags to 0 */
    memset(&state, 0, sizeof(addon_state_t));

    /* set the version number from git */
    char version_major[] = VERSION_MAJOR;
    char version_minor[] = VERSION_MINOR;
    char version_patch[] = VERSION_PATCH;
    state.data.version[0] = atoi(version_major) & 0xff;
    state.data.version[1] = atoi(version_minor) & 0xff;
    state.data.version[2] = atoi(version_patch) & 0xff;

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
    Delay_Ms(1000); // give serial monitor and SWD time to open

    PRINT("SystemClk: %u\r\n", (unsigned)SystemCoreClock);
    PRINT("ChipID: %08x\r\n", (unsigned)DBGMCU_GetCHIPID());

    /* configure the I2C pins and interrupts */
    IIC_Init(); // maps SWD lines to I2C
    /* configure the GPIO in- and outputs */
    Outputs_Init();

    /* configure the analog input reading using DMA */
    ADC_MultiChannel_Init();
    DMA_Tx_Init(ADC_DMA_CHANNEL, (u32)&ADC1->RDATAR, (u32)state.data.adc_channels, ADC_CHANNELS);
    DMA_Cmd(ADC_DMA_CHANNEL, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* configure the LCD backlight PWM output using DMA */
    LCD_PWM_Init(100, TIMER_FREQ, state.data.lcd_brightness);
    LCD_PWM_DMA_Init((u32)&state.data.lcd_brightness);
    TIM_DMACmd(LCD_BACKLIGHT_TIM, TIM_DMA_Update, ENABLE);
    TIM_Cmd(LCD_BACKLIGHT_TIM, ENABLE);
    TIM_CtrlPWMOutputs(LCD_BACKLIGHT_TIM, ENABLE);

    /* configure the Debug LED PWM output using DMA */
    LED_PWM_Init(100, TIMER_FREQ, state.data.led_brightness);
    TIM_Cmd(DEBUG_LED_TIM, ENABLE);
    TIM_CtrlPWMOutputs(DEBUG_LED_TIM, ENABLE);

    /* configure the button debounce timer */
    Button_Init(1, TIMER_FREQ);

    /* enable AUX power and unset the LCD reset pin */
    state.data.aux_power = 1;
    state.data.lcd_reset = 0;
    state.flag_update_outputs = 1;

    /* set the LCD backlight and LED at half brightness */
    state.data.lcd_brightness = 50;
    state.data.led_brightness = 50;
    state.flag_update_led = 1; /* there is no DMA for TIM3, so we use this flag to manually update */

    PRINT("Expander Init done\r\n");

    while (1)
    {
        /* this flag is set when we are halfway the next button scan */
        if (state.flag_button_scan_halfway)
        {
            state.flag_button_scan_halfway = 0;
            /* always turn off the interupt */
            GPIO_WriteBit(INT_OUTPUT_PORT, INT_OUTPUT_PIN, Bit_RESET);
        }

        if (state.flag_button_state_changed)
        {
            state.flag_button_state_changed = 0;
            /* notify the ESP32 that something has changed */
            GPIO_WriteBit(INT_OUTPUT_PORT, INT_OUTPUT_PIN, Bit_SET);
        }

        if (state.flag_update_outputs)
        {
            state.flag_update_outputs = 0;
            GPIO_WriteBit(AUX_POWER_PORT, AUX_POWER_PIN, state.data.aux_power ? Bit_SET : Bit_RESET);
            GPIO_WriteBit(LCD_RESET_PORT, LCD_RESET_PIN, state.data.lcd_reset ? Bit_SET : Bit_RESET);
            if (state.data.reboot)
            {
                PRINT("Reboot to bootloader trigger\r\n");
                Delay_Ms(100);
                reset_to_bootloader();
            }
        }

        /* there is no DMA for TIM3 so we set the compare value manually */
        if (state.flag_update_led)
        {
            state.flag_update_led = 0;
            DEBUG_LED_TIM_CVR = state.data.led_brightness;
        }
    }
}

/* interrupt handlers */
void TIM2_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_UP_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        /* Handle button scan */
        Button_Scan();
    }
    /* Clear interrupt flag */
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
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
