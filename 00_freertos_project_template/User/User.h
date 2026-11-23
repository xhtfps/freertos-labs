/**
 * @file User/User.h
 * @brief 用户应用公共类型、任务入口和测量命令/结果数据契约。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef USER_H
#define USER_H

/* FreeRTOS 核心头文件 */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* 硬件抽象层及标准库头文件 */
#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 测量命令类型枚举
 * @note  用于定义发送给测量任务的具体操作指令
 */
typedef enum
{
    USER_MEASURE_CMD_SINGLE = 0,        /**< 单次测量命令 */
    USER_MEASURE_CMD_CONTINUOUS_START,  /**< 启动连续测量 */
    USER_MEASURE_CMD_CONTINUOUS_STOP,   /**< 停止连续测量 */
    USER_MEASURE_CMD_GAIN_SET,          /**< 设置放大增益 */
    USER_MEASURE_CMD_PWM_ENABLE,        /**< 开启PWM输出 */
    USER_MEASURE_CMD_PWM_DISABLE        /**< 关闭PWM输出 */
} UserMeasureCommandType;

/**
 * @brief 测量命令结构体
 * @note  通过队列传递的命令数据包，包含命令类型及可选的参数
 */
typedef struct UserMeasureCommand
{
    UserMeasureCommandType type;        /**< 命令类型（见上方枚举） */
    uint8_t gain_code;                  /**< 增益代码（仅在命令为 GAIN_SET 时有效） */
} UserMeasureCommand;

/**
 * @brief 测量结果结构体
 * @note  用于存储和传递最终的测量数据（如超声波/激光测距结果）
 */
typedef struct UserMeasureResult
{
    uint8_t valid;                      /**< 数据有效性标志（1: 有效, 0: 无效/超时） */
    uint32_t echo_us;                   /**< 回波时间，单位：微秒 (µs) */
    float distance_mm;                  /**< 当前计算出的实际距离，单位：毫米 (mm) */
    float default_distance_mm;          /**< 默认/基准距离或校准值，单位：毫米 (mm) */
    uint8_t gain_code;                  /**< 采集该数据时所使用的增益代码 */
} UserMeasureResult;

/* FreeRTOS task handles */
extern TaskHandle_t user_task_handler;              /**< 用户主任务句柄 */
extern TaskHandle_t pwm_task_handler;               /**< PWM控制任务句柄 */
extern TaskHandle_t key_task_handler;               /**< 按键处理任务句柄 */

/* ========================================================================== */
/* API 函数声明                               */
/* ========================================================================== */

/**
 * @brief  初始化用户应用模块对象与底层运行资源
 * @note   在创建业务任务前调用，避免高优先级任务等待低优先级任务完成初始化
 * @retval 无
 */
void User_Init(void);

/**
 * @brief  用户主处理任务
 * @param  pvParameters: FreeRTOS 任务传入参数
 * @retval 无
 */
void User_Task(void *pvParameters);

/**
 * @brief  PWM 控制/驱动任务
 * @param  pvParameters: FreeRTOS 任务传入参数
 * @retval 无
 */
void PWM_Task(void *pvParameters);

/**
 * @brief  按键扫描与处理任务
 * @param  pvParameters: FreeRTOS 任务传入参数
 * @retval 无
 */
void Key_Task(void *pvParameters);

/**
 * @brief  用户初始化入口函数
 * @note   通常在 main.c 中内核启动前调用，用于创建队列、任务等资源
 * @retval 无
 */
void User_main(void);

/**
 * @brief  获取当前系统/测量模块是否处于忙碌状态
 * @retval 1: 忙碌（正在进行测量或处理）, 0: 空闲
 */
uint8_t User_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_H */
