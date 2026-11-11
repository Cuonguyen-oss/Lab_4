
#include "main.h"
#include "scheduler.h"
#include "output.h"

#include <stdio.h>

/* Peripheral handles */
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart2;

/* Function prototypes */
void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);


/* =====================================================
 * UART PRINT FUNCTION
 * Format: [10ms] task: 500 ms
 * ===================================================== */

static void uartPrintTimestamp(const char *tag)
{
    char str[64];

    int len = snprintf(
        str,
        sizeof(str),
        "%s%lu ms\r\n",
        tag,
        (unsigned long)get_time()
    );

    if (len > 0 && len < (int)sizeof(str))
    {
        HAL_UART_Transmit(
            &huart2,
            (uint8_t *)str,
            (uint16_t)len,
            100
        );
    }
}


/* =====================================================
 * LED EVENT PRINT FUNCTION
 * Format: LED_RED (0.5 s) at 500 ms
 * ===================================================== */

static void printLedEvent(
    const char *ledName,
    const char *interval
)
{
    char str[96];

    int len = snprintf(
        str,
        sizeof(str),
        "%s (%s) at %lu ms\r\n",
        ledName,
        interval,
        (unsigned long)get_time()
    );

    if (len > 0 && len < (int)sizeof(str))
    {
        HAL_UART_Transmit(
            &huart2,
            (uint8_t *)str,
            (uint16_t)len,
            100
        );
    }
}


/* =====================================================
 * FIVE PERIODIC LED TASKS
 * ===================================================== */

static void taskRed(void)
{
    ledRedToggle();

    printLedEvent("LED_RED", "0.5s");
}

static void taskYellow(void)
{
    ledYellowToggle();

    printLedEvent("LED_YELLOW", "1s");
}

static void taskGreen(void)
{
    ledGreenToggle();

    printLedEvent("LED_GREEN", "1.5s");
}

static void taskAqua(void)
{
    ledAquaToggle();

    printLedEvent("LED_AQUA", "2s");
}

static void taskBlue(void)
{
    ledBlueToggle();

    printLedEvent("LED_BLUE", "2.5 s");
}


/* =====================================================
 * PERIODIC 10 ms UART CALLBACK
 * ===================================================== */

static void timePrint10(void)
{
    uartPrintTimestamp("[10ms] task: ");
}


/* =====================================================
 * ONE-SHOT TASK
 * ===================================================== */

static void oneShotPrint(void)
{
    uartPrintTimestamp("[one-shot] ");
}


/* This task must never execute because it is deleted */
static void taskMustNotRun(void)
{
    uartPrintTimestamp("[ERROR] Deleted task executed at ");
}


/* =====================================================
 * ADD TASK WITH ERROR CHECKING
 * ===================================================== */

static uint32_t addTaskChecked(
    void (*pFunction)(void),
    uint32_t delay,
    uint32_t period
)
{
    uint32_t taskID = SCH_Add_Task(
        pFunction,
        delay,
        period
    );

    if (taskID == SCH_TASK_ERROR)
    {
        Error_Handler();
    }

    return taskID;
}


/* =====================================================
 * MAIN
 * ===================================================== */

int main(void)
{
    /* Initialize HAL */
    HAL_Init();

    /* Configure system clock */
    SystemClock_Config();

    /* Initialize peripherals */
    MX_GPIO_Init();

    MX_TIM2_Init();

    MX_USART2_UART_Init();


    /* Initialize scheduler */
    SCH_Init();

    /* =================================================
     * SECTION 1: ONE-SHOT TASKS
     * Register first to give them priority on equal deadlines
     * ================================================= */

    addTaskChecked(oneShotPrint, 3000, 0);
    addTaskChecked(oneShotPrint, 4000, 0);
    addTaskChecked(oneShotPrint, 5000, 0);


    /* =================================================
     * SECTION 2: FIVE PERIODIC LED TASKS
     * ================================================= */

    addTaskChecked(taskRed,    0,  500);
    addTaskChecked(taskYellow, 0, 1000);
    addTaskChecked(taskGreen,  0, 1500);
    addTaskChecked(taskAqua,   0, 2000);
    addTaskChecked(taskBlue,   0, 2500);


    /* =================================================
     * SECTION 3: 10 ms PERIODIC CALLBACK
     * ================================================= */

    addTaskChecked(timePrint10, 10, 10);


    /* =================================================
     * SECTION 4: DELETE TASK TEST
     * ================================================= */

    uint32_t deletedTaskID = addTaskChecked(
        taskMustNotRun,
        6000,
        0
    );

    if (SCH_Delete_Task(deletedTaskID) == 0)
    {
        Error_Handler();
    }


    /* =================================================
     * SECTION 5: EXECUTE IMMEDIATE TASKS
     *
     * Initial delay = 0 ms
     * Expected execution time = 0 ms
     * ================================================= */

    SCH_Dispatch_Tasks();


    /* =================================================
     * START TIM2 INTERRUPT
     * ================================================= */

    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
    {
        Error_Handler();
    }


    /* =================================================
     * SUPER LOOP
     * ================================================= */

    while (1)
    {
        SCH_Dispatch_Tasks();
    }
}


/* =====================================================
 * SYSTEM CLOCK
 *
 * HSI = 8 MHz
 * ===================================================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};

    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_NONE;


    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }


    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;


    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}


/* =====================================================
 * GPIO INITIALIZATION
 * ===================================================== */

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();


    /* Initial LED state */
    HAL_GPIO_WritePin(
        GPIOA,
        LED_RED_Pin |
        LED_YELLOW_Pin |
        LED_GREEN_Pin |
        LED_AQUA_Pin |
        LED_BLUE_Pin,
        GPIO_PIN_RESET
    );


    /* Configure LED pins */
    GPIO_InitStruct.Pin =
        LED_RED_Pin |
        LED_YELLOW_Pin |
        LED_GREEN_Pin |
        LED_AQUA_Pin |
        LED_BLUE_Pin;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull = GPIO_NOPULL;

    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}


/* =====================================================
 * TIM2 INITIALIZATION
 *
 * TIM2 Clock = 8 MHz
 * Prescaler  = 1
 * Period     = 39999
 *
 * T = (1+1)(39999+1)/8000000
 *   = 10 ms
 * ===================================================== */

static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};

    TIM_MasterConfigTypeDef sMasterConfig = {0};


    htim2.Instance = TIM2;

    htim2.Init.Prescaler = 1;

    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;

    htim2.Init.Period = 39999;

    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

    htim2.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;


    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }


    sClockSourceConfig.ClockSource =
        TIM_CLOCKSOURCE_INTERNAL;

    if (HAL_TIM_ConfigClockSource(
            &htim2,
            &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }


    sMasterConfig.MasterOutputTrigger =
        TIM_TRGO_RESET;

    sMasterConfig.MasterSlaveMode =
        TIM_MASTERSLAVEMODE_DISABLE;


    if (HAL_TIMEx_MasterConfigSynchronization(
            &htim2,
            &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}


/* =====================================================
 * USART2 INITIALIZATION
 *
 * Baud rate = 115200
 * 8 data bits
 * No parity
 * 1 stop bit
 * ===================================================== */

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;

    huart2.Init.BaudRate = 115200;

    huart2.Init.WordLength = UART_WORDLENGTH_8B;

    huart2.Init.StopBits = UART_STOPBITS_1;

    huart2.Init.Parity = UART_PARITY_NONE;

    huart2.Init.Mode = UART_MODE_TX_RX;

    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;

    huart2.Init.OverSampling = UART_OVERSAMPLING_16;


    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}


/* =====================================================
 * TIMER INTERRUPT CALLBACK
 * ===================================================== */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        SCH_Update();
    }
}


/* =====================================================
 * ERROR HANDLER
 * ===================================================== */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}


#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    /* User implementation */
}

#endif
