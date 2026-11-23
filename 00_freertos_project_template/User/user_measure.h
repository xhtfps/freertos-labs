/**
 * @file User/user_measure.h
 * @brief 超声测距服务状态机、PWM 触发和回波采集模块。
 * @details
 * - 封装测量命令队列、结果队列、任务通知和中断共享状态。
 * - UI 通过客户端接口调用测量能力，不直接依赖 PWM、PGA 或 EXTI 细节。
 * - 中断路径只记录时间戳和完成标志，耗时处理交给测量任务。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef USER_MEASURE_H
#define USER_MEASURE_H

#include "user_calibration.h"
#include "user_history.h"
#include "stm32f4xx_hal.h"

/* * 补充外部依赖类型的占位或前置包含，确保头文件独立编译通过。
 * 假设 UserMeasureCommand 和 UserMeasureResult 定义在基础结构头文件中。
 */
// #include "user_types.h" 
// #include "FreeRTOS.h"
// #include "queue.h"
// #include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前置声明：用于解决结构体相互引用和指针类型未定义的问题 */
typedef struct UserMeasureService UserMeasureService;

/**
 * @brief 测量服务模块的“虚函数表” (Operations)
 * @details 对外暴露的控制与数据获取接口，隔离了硬件实现细节
 */
typedef struct UserMeasureServiceOps
{
    /** @brief 初始化测量服务的硬件（GPIO/Timer/PGA）及 RTOS 资源（队列） */
    void (*init)(UserMeasureService *self);
    
    /** @brief 测量服务主状态机循环（常驻于独立的 PWM_Task 中） */
    void (*run)(UserMeasureService *self);
    
    /** @brief 异步发送控制指令（如开启连续测量、设置增益等，非阻塞） */
    void (*send_command)(UserMeasureService *self, const UserMeasureCommand *cmd);
    
    /** * @brief 发起单次精准测量（阻塞式同步调用）
     * @param timeout RTOS 阻塞超时时间
     * @return 1: 测量成功返回有效数据; 0: 测量超时或失败
     */
    uint8_t (*request_single)(UserMeasureService *self, UserMeasureResult *result, TickType_t timeout);
    
    /** * @brief 提取测量结果（通常在连续测量模式下，由 UI 任务轮询调用）
     * @param timeout 队列读取等待时间
     * @return 1: 成功获取最新结果; 0: 队列为空
     */
    uint8_t (*receive_result)(UserMeasureService *self, UserMeasureResult *result, TickType_t timeout);
    
    /** @brief 查询底层硬件目前是否处于测量忙碌状态 */
    uint8_t (*is_busy)(const UserMeasureService *self);
} UserMeasureServiceOps;

/**
 * @brief 测量服务类实体结构体
 * @details 包含了多线程/中断环境下所需的各类同步句柄与状态寄存器
 */
struct UserMeasureService
{
    const UserMeasureServiceOps *ops;   // 虚函数表指针

    /* === RTOS 并发与通信资源 === */
    QueueHandle_t cmd_queue;            // 接收上层 UI 或业务逻辑发来的指令队列
    QueueHandle_t result_queue;         // 向上层推送连续测量结果的数据队列
    TaskHandle_t waiting_task;          // 记录发起 request_single 的任务句柄，用于 ISR 或底层唤醒
    
    /* === 依赖注入 === */
    UserCalibration *calibration;       // 注入校准算法模块（用于将 us 转换为精确 mm）
    UserResultHistory *history;         // 注入历史记录模块

    /* === 硬件中断与任务共享状态 (关键数据加 volatile 防优化) === */
    volatile uint8_t measure_active;    // 标记当前是否处于一次有效的测距周期中
    volatile uint8_t echo_rise_seen;    // 标记是否已经捕获到超声波回波的上升沿
    volatile uint32_t echo_rise_us;     // 记录回波上升沿发生时的硬件微秒时间戳
    volatile uint32_t echo_time_us;     // 计算得出的最终回波脉宽（微秒）
    volatile uint8_t busy;              // 全局忙碌锁状态标志

    /* === 硬件配置与上下文数据 === */
    uint8_t gain_code;                  // 当前 PGA112 放大器的增益配置字
    uint8_t gain_settle_discard;        // 切换增益后需要丢弃的不稳定测量次数
    uint32_t last_echo_us;              // 缓存上一次成功测得的回波时间
};

/** @brief 声明全局唯一的测量服务实例 */
extern UserMeasureService g_user_measure_service;

/**
 * @brief 测量服务构造函数
 * @param self 指向待构造的实体
 * @param calibration 注入的校准模块指针
 * @param history 注入的历史模块指针
 */
void UserMeasureService_Construct(UserMeasureService *self,
                                  UserCalibration *calibration,
                                  UserResultHistory *history);

/**
 * @brief 外部硬件中断（EXTI / Timer Input Capture）的回调入口
 * @details 
 * 极高频/高优先级执行。负责记录微秒时间戳，并计算超声波飞行时间。
 * @param now_us 当前定时器读取到的绝对微秒时间
 * @param level 当前触发中断的引脚电平状态 (上升沿/下降沿)
 */
void UserMeasureService_EchoCallback(uint32_t now_us, GPIO_PinState level);

#ifdef __cplusplus
}
#endif

#endif /* USER_MEASURE_H */
