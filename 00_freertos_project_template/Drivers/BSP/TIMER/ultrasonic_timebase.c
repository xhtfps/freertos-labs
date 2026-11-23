/**
 * @file Drivers/BSP/TIMER/ultrasonic_timebase.c
 * @brief 超声测距微秒级时基对象模块。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/TIMER/ultrasonic_timebase.h"

static void UltrasonicTimebase_Init(UltrasonicTimebase *self);
static void UltrasonicTimebase_Reset(UltrasonicTimebase *self);
static uint32_t UltrasonicTimebase_GetUs(UltrasonicTimebase *self);

static const UltrasonicTimebaseOps s_ultrasonic_timebase_ops =
{
    UltrasonicTimebase_Init,
    UltrasonicTimebase_Reset,
    UltrasonicTimebase_GetUs
};

UltrasonicTimebase g_ultrasonic_timebase =
{
    {0},
    &s_ultrasonic_timebase_ops
};

static void UltrasonicTimebase_Init(UltrasonicTimebase *self)
{
    if (self == NULL)
    {
        return;
    }

    __HAL_RCC_TIM5_CLK_ENABLE();

    self->handle.Instance = TIM5;
    self->handle.Init.Prescaler = 84U - 1U;
    self->handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    self->handle.Init.Period = 0xFFFFFFFFU;
    self->handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    self->handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&self->handle);
    HAL_TIM_Base_Start(&self->handle);
}

static void UltrasonicTimebase_Reset(UltrasonicTimebase *self)
{
    if (self == NULL)
    {
        return;
    }

    __HAL_TIM_SET_COUNTER(&self->handle, 0U);
}

static uint32_t UltrasonicTimebase_GetUs(UltrasonicTimebase *self)
{
    if (self == NULL)
    {
        return 0U;
    }

    return __HAL_TIM_GET_COUNTER(&self->handle);
}
