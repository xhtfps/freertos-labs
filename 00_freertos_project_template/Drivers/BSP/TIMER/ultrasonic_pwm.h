/**
 * @file Drivers/BSP/TIMER/ultrasonic_pwm.h
 * @brief 超声发射 PWM 脉冲串对象模块。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __ULTRASONIC_PWM_H
#define __ULTRASONIC_PWM_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UltrasonicPwm UltrasonicPwm;

/**
 * @brief 超声发射 PWM 对象操作表。
 * @details 支持持续使能、强制关闭和固定周期数脉冲串三种典型控制方式。
 */
typedef struct
{
    void (*init)(UltrasonicPwm *self);       /**< 初始化 PWM 定时器、通道和收发 GPIO。 */
    void (*enable)(UltrasonicPwm *self);     /**< 持续开启 PWM 输出。 */
    void (*disable)(UltrasonicPwm *self);    /**< 立即关闭 PWM 并恢复接收状态。 */
    void (*fire_burst)(UltrasonicPwm *self); /**< 发射固定周期数的脉冲串。 */
    uint8_t (*is_active)(UltrasonicPwm *self); /**< 查询发射链路是否正在工作。 */
    void (*handle_irq)(UltrasonicPwm *self); /**< 定时器中断入口，用于统计脉冲周期。 */
} UltrasonicPwmOps;

/**
 * @brief 超声 PWM 发射对象。
 * @details active/burst_pending/cycle_count 会在任务与中断之间共享，因此使用 volatile。
 */
struct UltrasonicPwm
{
    TIM_HandleTypeDef *handle;       /**< PWM 使用的 HAL TIM 句柄。 */
    volatile uint8_t active;         /**< 当前是否处于发射状态。 */
    volatile uint8_t burst_pending;  /**< 是否正在执行有限周期脉冲串。 */
    volatile uint16_t cycle_count;   /**< 已发射 PWM 周期计数。 */
    const UltrasonicPwmOps *ops;     /**< PWM 发射方法表。 */
};

extern TIM_HandleTypeDef g_ultrasonic_pwm_tim;
extern UltrasonicPwm g_ultrasonic_pwm;

#ifdef __cplusplus
}
#endif

#endif /* __ULTRASONIC_PWM_H */
