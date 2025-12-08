#include <ch32x035.h> /* both X033 and X035 */
#include <stdio.h>
#include <debug.h>

// ananlog pins
#define BATTERY_MONITOR_PORT GPIOC // PC0: Battery Monitor
#define BATTERY_MONITOR_PIN GPIO_Pin_0
#define USB_MONITOR_PORT GPIOC // PC3: USB Monitor
#define USB_MONITOR_PIN GPIO_Pin_0
#define JOYSTICK_Y_PORT GPIOA // PA5: JoyY
#define JOYSTICK_Y_PIN GPIO_Pin_5
#define JOYSTICK_X_PORT GPIOA // PA6: JoyX
#define JOYSTICK_X_PIN GPIO_Pin_6
#define ADC_CHANNEL_TO_USE ADC_Channel_0

// digital inputs
#define CHARGER_CHARGING_PORT GPIOA // PA2: Charger, charging
#define CHARGER_CHARGING_PIN GPIO_Pin_2
#define CHARGER_STANDBY_PORT GPIOA // PA3: Charger, standby
#define CHARGER_STANDBY_PIN GPIO_Pin_3
#define BUTTON_X_PORT GPIOB // PB7: X Button
#define BUTTON_X_PIN GPIO_Pin_7
#define BUTTON_A_PORT GPIOB // PB8: A Button
#define BUTTON_A_PIN GPIO_Pin_8
#define BUTTON_B_PORT GPIOB // PB9: B Button
#define BUTTON_B_PIN GPIO_Pin_9
#define BUTTON_Y_PORT GPIOB // PB10: Y Button
#define BUTTON_Y_PIN GPIO_Pin_10
#define BUTTON_MENU_PORT GPIOC // PC15: Menu Button
#define BUTTON_MENU_PIN GPIO_Pin_15

// digital outputs
#define AUX_POWER_PORT GPIOB // PB6: Aux power
#define AUX_POWER_PIN GPIO_Pin_6
#define LCD_RESET_PORT GPIOB // PB11: LCD reset
#define LCD_RESET_PIN GPIO_Pin_11
#define INT_OUTPUT_PORT GPIOC // PC16: Interrupt output
#define INT_OUTPUT_PIN GPIO_Pin_16

// PWM
#define DEBUG_LED_PORT GPIOA // PA4: debug LED
#define DEBUG_LED_PIN GPIO_Pin_4
#define DEBUG_LED_TIM TIM3 // mapping 0b11
#define DEBUG_LED_CHAN TIM_Channel_2
#define BUZZER_PORT GPIOC // PC14: Buzzer
#define BUZZER_PIN GPIO_Pin_14
#define BUZZER_TIM TIM2 // mapping 0b11x
#define BUZZER_CHAN TIM_Channel_2
#define LCD_BACKLIGHT_PORT GPIOB // PB12: LCD BL
#define LCD_BACKLIGHT_PIN GPIO_Pin_12
#define LCD_BACKLIGHT_TIM TIM1 // mapping 0b010
#define LCD_BACKLIGHT_CHAN TIM_Channel_4

// I2C
#define SDA_PORT GPIOC
#define SDA_PIN GPIO_Pin_18
#define SCL_PORT GPIOC
#define SCL_PIN GPIO_Pin_19
#define I2C_ADDRESS (0x38)

// static void Interrupt_setup()
// {
// }

static void Uart_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx; // TX only

    USART_Init(USART4, &USART_InitStructure);
    USART_Cmd(USART4, ENABLE);
}

static void Gpio_Init(void)
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

    // inputs
    GPIO_InitStructure.GPIO_Pin = CHARGER_CHARGING_PIN | CHARGER_STANDBY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = BUTTON_X_PIN | BUTTON_A_PIN | BUTTON_B_PIN | BUTTON_Y_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = BUTTON_MENU_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUTTON_MENU_PORT, &GPIO_InitStructure);
}

static void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef I2C_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    GPIO_PinRemapConfig(GPIO_PartialRemap3_I2C1, ENABLE); // 011: Mapping (SCL/PC19, SDA/PC18)
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);  // Disable DIO (SWD) interface

    GPIO_InitStructure.GPIO_Pin = SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // automatic open-drain
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SDA_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SCL_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // automatic open-drain
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SCL_PORT, &GPIO_InitStructure);

    I2C_InitStructure.I2C_ClockSpeed = 400000;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_16_9;
    I2C_InitStructure.I2C_OwnAddress1 = I2C_ADDRESS << 1; // 7 or 10 bit address
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitStructure);

    I2C_Cmd(I2C1, ENABLE);
}

static void PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = DEBUG_LED_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DEBUG_LED_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = LCD_BACKLIGHT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_BACKLIGHT_PORT, &GPIO_InitStructure);

    // Enable timers
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3, ENABLE);

    // set pinmux
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);     // set debug LED (PA4) on CH2 of TIM3
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, ENABLE);     // set Buzzer (PC14) on CH2 of TIM2
    GPIO_PinRemapConfig(GPIO_PartialRemap2_TIM1, ENABLE); // set LCD backlight (PB12) on CH4 of TIM1

    // TIM1 : LCD backlight
    TIM_TimeBaseInitStructure.TIM_Period = 100;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 480 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);

#if (PWM_MODE == PWM_MODE1)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
#elif (PWM_MODE == PWM_MODE2)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
#endif

    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 75;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);

    // TIM2: Buzzer
    TIM_TimeBaseInitStructure.TIM_Period = 100;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 480 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

#if (PWM_MODE == PWM_MODE1)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
#elif (PWM_MODE == PWM_MODE2)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
#endif

    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 75;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    // TIM3 : debug LED
    TIM_TimeBaseInitStructure.TIM_Period = 100;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 480 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

#if (PWM_MODE == PWM_MODE1)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
#elif (PWM_MODE == PWM_MODE2)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
#endif

    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 75;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM3, ENABLE);
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

static void ADC_Function_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    // Determines clock for ADC.
    // At PCLK2 = 48MHz, ADCCLOCK = PCLK2 / 8 = 6MHz
    // RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    GPIO_InitStructure.GPIO_Pin = BATTERY_MONITOR_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(BATTERY_MONITOR_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = USB_MONITOR_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(USB_MONITOR_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = JOYSTICK_Y_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(JOYSTICK_Y_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = JOYSTICK_X_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(JOYSTICK_X_PORT, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    // ADC_BufferCmd(ADC1, ENABLE); // enable buffer
}

static u16 Get_ADC_Val(u8 ch)
{
    u16 val = 0;
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_11Cycles); // or 7?
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
        ;
    val = ADC_GetConversionValue(ADC1);
    return val;
}

static u16 Get_ADC_Average(u8 ch, u8 times)
{
    u32 temp_val = 0;
    u8 t;
    u16 val;

    for (t = 0; t < times; t++)
    {
        temp_val += Get_ADC_Val(ch);
        Delay_Ms(5);
    }

    val = temp_val / times;

    return val;
}

int main(void)
{
    u16 ADC_val;

#ifdef NVIC_PriorityGroup_2
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
#else
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
#endif
    SystemCoreClockUpdate();
    Delay_Init();
    Uart_Init();

    Delay_Ms(1000); // give serial monitor time to open

    printf("SystemClk: %u\r\n", (unsigned)SystemCoreClock);
    printf("ChipID: %08x\r\n", (unsigned)DBGMCU_GetCHIPID());

    Gpio_Init();
    IIC_Init();
    PWM_Init();
    ADC_Function_Init();

    printf("Init done\r\n");

    uint8_t ledState = 0;
    while (1)
    {
        ADC_val = Get_ADC_Average(ADC_CHANNEL_TO_USE, 10);
        printf("ADC val: %d\r\n", ADC_val);
        // GPIO_WriteBit(DEBUG_LED_GPIO_PORT, DEBUG_LED_GPIO_PIN, ledState);
        ledState ^= 1; // invert for the next run
        Delay_Ms(1000);
    }
}

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void NMI_Handler(void) {}
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void)
{
    while (1)
    {
    }
}
