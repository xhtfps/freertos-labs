/**
 * @file Drivers/BSP/TIMER/ultrasonic_timer.c
 * @brief 超声定时器兼容 API 聚合层。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/TIMER/ultrasonic_timer.h"
#include "./BSP/TIMER/ultrasonic_timebase.h"
#include "./BSP/TIMER/ultrasonic_pwm.h"

void ultrasonic_timebase_init(void)
{
    g_ultrasonic_timebase.ops->init(&g_ultrasonic_timebase);
}

void ultrasonic_timebase_reset(void)
{
    g_ultrasonic_timebase.ops->reset(&g_ultrasonic_timebase);
}

uint32_t ultrasonic_timebase_get_us(void)
{
    return g_ultrasonic_timebase.ops->get_us(&g_ultrasonic_timebase);
}

void Ultrasonic_PWM_Init(void)
{
    g_ultrasonic_pwm.ops->init(&g_ultrasonic_pwm);
}

void Ultrasonic_PWM_OutputEnable(void)
{
    g_ultrasonic_pwm.ops->enable(&g_ultrasonic_pwm);
}

void Ultrasonic_PWM_OutputDisable(void)
{
    g_ultrasonic_pwm.ops->disable(&g_ultrasonic_pwm);
}

void Ultrasonic_FireBurst(void)
{
    g_ultrasonic_pwm.ops->fire_burst(&g_ultrasonic_pwm);
}

uint8_t ultrasonic_pwm_is_active(void)
{
    return g_ultrasonic_pwm.ops->is_active(&g_ultrasonic_pwm);
}
