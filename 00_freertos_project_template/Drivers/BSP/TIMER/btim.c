/**
 * @file Drivers/BSP/TIMER/btim.c
 * @brief 基础定时器运行时统计服务模块。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/LED/led.h"
#include "./BSP/TIMER/btim.h"
#include "./SYSTEM/usart/usart.h"

TIM_HandleTypeDef tim3_handler;         /* 定时器参数句柄 */

uint32_t FreeRTOSRunTimeTicks;

static void BasicTimer_Init(BasicTimer *self, uint16_t arr, uint16_t psc);
static void BasicTimer_ResetRuntimeStats(BasicTimer *self);
static void BasicTimer_HandleIrq(BasicTimer *self);
static void BasicTimer_HandlePeriodElapsed(BasicTimer *self, TIM_HandleTypeDef *htim);
static void BasicTimer_Construct(BasicTimer *self);

static const BasicTimerOps s_basic_timer_ops =
{
    BasicTimer_Init,
    BasicTimer_ResetRuntimeStats,
    BasicTimer_HandleIrq,
    BasicTimer_HandlePeriodElapsed
};

BasicTimer g_btim3_runtime_timer;

static void BasicTimer_Construct(BasicTimer *self)
{
    if ((self == NULL) || (self->ops != NULL))
    {
        return;
    }

    self->handle = &tim3_handler;
    self->instance = BTIM_TIM3_INT;
    self->irq = BTIM_TIM3_INT_IRQn;
    self->runtime_ticks = &FreeRTOSRunTimeTicks;
    self->ops = &s_basic_timer_ops;
}

/* 配置定时器以用于运行时间统计 */
void ConfigureTimeForRunTimeStats(void)
{
    BasicTimer_Construct(&g_btim3_runtime_timer);
    g_btim3_runtime_timer.ops->reset_runtime_stats(&g_btim3_runtime_timer);
}

/**
 * @brief       基本定时器TIM3中断服务函数
 * @param       无
 * @retval      无
 */
void BTIM_TIM3_INT_IRQHandler(void)
{
    BasicTimer_Construct(&g_btim3_runtime_timer);
    g_btim3_runtime_timer.ops->handle_irq(&g_btim3_runtime_timer);  /*定时器回调函数*/
}

/**
 * @brief       回调函数，定时器中断服务函数调用
 * @param       无
 * @retval      无
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    BasicTimer_Construct(&g_btim3_runtime_timer);
    g_btim3_runtime_timer.ops->handle_period_elapsed(&g_btim3_runtime_timer, htim);
}

/**
 * @brief       基本定时器TIMX定时中断初始化函数
 * @note
 *              基本定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              基本定时器的时钟为APB1时钟的2倍, 而APB1为36M, 所以定时器时钟 = 72Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft=定时器工作频率,单位:Mhz
 *
 * @param       arr: 自动重装值。
 * @param       psc: 时钟预分频数
 * @retval      无
 */
static void BasicTimer_Init(BasicTimer *self, uint16_t arr, uint16_t psc)
{
    if ((self == NULL) || (self->handle == NULL))
    {
        return;
    }

    self->handle->Instance = self->instance;                    /*通用定时器X*/
    self->handle->Init.Prescaler = psc;                         /* 设置预分频器  */
    self->handle->Init.CounterMode = TIM_COUNTERMODE_UP;        /* 向上计数器 */
    self->handle->Init.Period = arr;                            /*自动装载值*/
    self->handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;  /*时钟分频因子*/
    HAL_TIM_Base_Init(self->handle);

    HAL_TIM_Base_Start_IT(self->handle);                        /*使能通用定时器x和及其更新中断：TIM_IT_UPDATE*/
}

static void BasicTimer_ResetRuntimeStats(BasicTimer *self)
{
    if ((self == NULL) || (self->runtime_ticks == NULL))
    {
        return;
    }

    *(self->runtime_ticks) = 0U;
}

static void BasicTimer_HandleIrq(BasicTimer *self)
{
    if ((self == NULL) || (self->handle == NULL))
    {
        return;
    }

    HAL_TIM_IRQHandler(self->handle);
}

static void BasicTimer_HandlePeriodElapsed(BasicTimer *self, TIM_HandleTypeDef *htim)
{
    if ((self == NULL) || (self->handle == NULL) || (self->runtime_ticks == NULL))
    {
        return;
    }

    if (htim == self->handle)
    {
        (*(self->runtime_ticks))++;  /* 运行时间统计计数器递增 */
    }
}

void btim_tim3_int_init(uint16_t arr, uint16_t psc)
{
    BasicTimer_Construct(&g_btim3_runtime_timer);
    g_btim3_runtime_timer.ops->init(&g_btim3_runtime_timer, arr, psc);
}

/**
 * @brief       定时器层驱动，开启时钟，设置中断优先级
                此函数会被HAL_TIM_Base_Init()函数调用
 * @param       无
 * @retval      无
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == BTIM_TIM3_INT)
    {
        BTIM_TIM3_INT_CLK_ENABLE();                     /*使能TIM时钟*/
        HAL_NVIC_SetPriority(BTIM_TIM3_INT_IRQn, 6, 0); /* 抢占1，子优先级3，组2 */
        HAL_NVIC_EnableIRQ(BTIM_TIM3_INT_IRQn);         /*开启ITM3中断*/
    }
}
