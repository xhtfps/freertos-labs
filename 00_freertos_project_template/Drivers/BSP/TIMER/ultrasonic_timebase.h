/**
 * @file Drivers/BSP/TIMER/ultrasonic_timebase.h
 * @brief 超声测距微秒级时基对象模块。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __ULTRASONIC_TIMEBASE_H
#define __ULTRASONIC_TIMEBASE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UltrasonicTimebase UltrasonicTimebase;

/**
 * @brief 超声测距微秒时基操作表。
 * @details 提供独立于 PWM 的 1MHz 计数器，用作回波边沿时间戳来源。
 */
typedef struct
{
    void (*init)(UltrasonicTimebase *self);   /**< 初始化并启动 1MHz 定时器。 */
    void (*reset)(UltrasonicTimebase *self);  /**< 将计数值清零，通常在发射前调用。 */
    uint32_t (*get_us)(UltrasonicTimebase *self); /**< 读取当前微秒计数。 */
} UltrasonicTimebaseOps;

/**
 * @brief 超声测距微秒时基对象。
 */
struct UltrasonicTimebase
{
    TIM_HandleTypeDef handle;             /**< 时基定时器 HAL 句柄。 */
    const UltrasonicTimebaseOps *ops;     /**< 时基方法表。 */
};

extern UltrasonicTimebase g_ultrasonic_timebase;

#ifdef __cplusplus
}
#endif

#endif /* __ULTRASONIC_TIMEBASE_H */
