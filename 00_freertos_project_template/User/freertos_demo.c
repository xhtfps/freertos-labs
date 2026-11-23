/**
 * @file User/freertos_demo.c
 * @brief FreeRTOS 调度启动、任务创建与低功耗钩子实现。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "freertos_demo.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./TFT_LCD/TFT_LCD.h"
#include "./BSP/PS2/ps2.h"
#include "./SYSTEM/delay/delay.h"
#include "./MALLOC/malloc.h"
#include "User.h"

/* FreeRTOS 核心组件头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "semphr.h"

/******************************************************************************************************/
/* FreeRTOS 任务配置 */

/* START_TASK (启动任务) 配置 
 * 优先级: 1 (较低) | 堆栈大小: 128 字 | 作用: 用于创建其他业务任务，随后自我删除
 */
#define START_TASK_PRIO         1
#define START_TASK_STACK_SIZE    128
TaskHandle_t start_task_handler;
void start_task(void * pvParameters);

/* USER_TASK (用户主业务任务) 配置 
 * 优先级: 2 | 堆栈大小: 768 字 | 作用: 处理核心业务逻辑、数据采集等
 */
#define USER_TASK_PRIO          2
#define USER_TASK_STACK_SIZE    768
TaskHandle_t user_task_handler;

/* PWM_TASK (PWM驱动控制任务) 配置 
 * 优先级: 3 | 堆栈大小: 512 字 | 作用: 负责控制脉宽调制输出或定时驱动
 */
#define PWM_TASK_PRIO          3
#define PWM_TASK_STACK_SIZE    512
TaskHandle_t pwm_task_handler;

/* KEY_TASK (按键检测任务) 配置 
 * 优先级: 4 (最高) | 堆栈大小: 256 字 | 作用: 负责按键扫描与去抖，高优先级保证响应实时性
 */
#define KEY_TASK_PRIO          4
#define KEY_TASK_STACK_SIZE    256
TaskHandle_t key_task_handler;

/******************************************************************************************************/

/* 全局同步机制句柄声明 */
SemaphoreHandle_t semphore_handle;  /**< 二值/计数信号量句柄 (原为 QueueHandle_t，已更正为标准信号量类型) */


/**
 * @brief  进入低功耗模式前的预处理函数 (Tickless Idle 钩子函数)
 * @note   在系统即将进入睡眠模式时由 FreeRTOS 内核自动调用。
 * 作用是检查系统忙碌状态，并在进入睡眠时关闭不需要的外设时钟以最大化省电。
 * @param  expected_idle_time: 指向期望空闲时间的指针 (单位: Tick)
 * @retval 无
 */
void PRE_SLEEP_PROCESSING(uint32_t *expected_idle_time)
{
    /* 检查用户业务模块是否正忙（例如：正在进行持续测量或数据传输） */
    if (User_IsBusy() != 0U)
    {
        if (expected_idle_time != NULL)
        {
            *expected_idle_time = 0U; /* 强制将空闲时间清零，阻止内核进入低功耗模式 */
        }
        return;
    }

    /* 关闭各个外设在低功耗睡眠模式下的时钟 (SLEEP_DISABLE)，防止睡眠时外设继续耗电 */
    __HAL_RCC_DMA1_CLK_SLEEP_DISABLE();
    __HAL_RCC_DMA2_CLK_SLEEP_DISABLE();
    __HAL_RCC_ADC1_CLK_SLEEP_DISABLE();
    __HAL_RCC_SPI1_CLK_SLEEP_DISABLE();
    __HAL_RCC_I2C1_CLK_SLEEP_DISABLE();
    __HAL_RCC_CAN1_CLK_SLEEP_DISABLE();
    __HAL_RCC_TIM2_CLK_SLEEP_DISABLE();
    __HAL_RCC_TIM3_CLK_SLEEP_DISABLE();
    __HAL_RCC_TIM4_CLK_SLEEP_DISABLE();
    __HAL_RCC_TIM5_CLK_SLEEP_DISABLE();
    __HAL_RCC_USART1_CLK_SLEEP_DISABLE();
    __HAL_RCC_PWR_CLK_SLEEP_DISABLE();
}

/**
 * @brief  退出低功耗模式后的后处理函数 (Tickless Idle 钩子函数)
 * @note   当 MCU 被外部中断唤醒并退出睡眠模式时，由 FreeRTOS 内核在第一时间调用。
 * 作用是重新使能各个外设在睡眠模式下的时钟，恢复外设的正常运行。
 * @retval 无
 */
void POST_SLEEP_PROCESSING(void)
{
    /* 重新使能各个外设在睡眠模式下的时钟 */
    __HAL_RCC_DMA1_CLK_SLEEP_ENABLE();
    __HAL_RCC_DMA2_CLK_SLEEP_ENABLE();
    __HAL_RCC_ADC1_CLK_SLEEP_ENABLE();
    __HAL_RCC_SPI1_CLK_SLEEP_ENABLE();
    __HAL_RCC_I2C1_CLK_SLEEP_ENABLE();
    __HAL_RCC_CAN1_CLK_SLEEP_ENABLE();
    __HAL_RCC_TIM2_CLK_SLEEP_ENABLE();
    __HAL_RCC_TIM3_CLK_SLEEP_ENABLE();
    __HAL_RCC_TIM4_CLK_SLEEP_ENABLE();
    __HAL_RCC_TIM5_CLK_SLEEP_ENABLE();
    __HAL_RCC_USART1_CLK_SLEEP_ENABLE();
    __HAL_RCC_PWR_CLK_SLEEP_ENABLE();
}

/**
 * @brief  FreeRTOS 演示入口函数
 * @note   由外部 main.c 调用。负责创建启动任务并开启 FreeRTOS 任务调度器。
 * @retval 无
 */
void freertos_demo(void)
{
    /* 创建启动任务 (start_task) */
    xTaskCreate ((TaskFunction_t        ) start_task,
                (char * ) "start_task",
                (configSTACK_DEPTH_TYPE ) START_TASK_STACK_SIZE,
                (void * ) NULL,
                (UBaseType_t            ) START_TASK_PRIO,
                (TaskHandle_t * ) &start_task_handler );
                
    /* 启动 FreeRTOS 任务调度器 (此后代码不再返回，CPU 掌控权交由 FreeRTOS) */
    vTaskStartScheduler();
}

/**
 * @brief  启动任务的实现函数
 * @note   在内核启动后第一个运行。负责创建所有实际的业务任务，并在创建完成后删除自身。
 * @param  pvParameters: 任务传入参数 (此处未使用，为 NULL)
 * @retval 无
 */
void start_task(void * pvParameters)
{
    User_Init();                /* 先完成模块对象和硬件资源初始化，避免高优先级任务空等 */

    taskENTER_CRITICAL();       /* 进入临界区，防止任务创建过程中被高优先级中断或任务打断 */

    /* 创建 用户主业务任务 */
    xTaskCreate ((TaskFunction_t        ) User_Task,
                (char * ) "user_task",
                (configSTACK_DEPTH_TYPE ) USER_TASK_STACK_SIZE,
                (void * ) NULL,
                (UBaseType_t            ) USER_TASK_PRIO,
                (TaskHandle_t * ) &user_task_handler );

    /* 创建 PWM驱动控制任务 */
    xTaskCreate ((TaskFunction_t        ) PWM_Task,
                (char * ) "pwm_task",
                (configSTACK_DEPTH_TYPE ) PWM_TASK_STACK_SIZE,
                (void * ) NULL,
                (UBaseType_t            ) PWM_TASK_PRIO,
                (TaskHandle_t * ) &pwm_task_handler );

    /* 创建 按键检测任务 */
    xTaskCreate ((TaskFunction_t        ) Key_Task,
                (char * ) "key_task",
                (configSTACK_DEPTH_TYPE ) KEY_TASK_STACK_SIZE,
                (void * ) NULL,
                (UBaseType_t            ) KEY_TASK_PRIO,
                (TaskHandle_t * ) &key_task_handler );

    taskEXIT_CRITICAL();        /* 退出临界区，恢复正常的任务调度机制 */
    
    /* 业务任务创建完毕，删除启动任务自身以释放堆栈内存 */
    vTaskDelete(NULL);
}
