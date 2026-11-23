/**
 * @file Drivers/BSP/TIMER/btim.h
 * @brief 基础定时器运行时统计服务模块。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __BTIM_H
#define __BTIM_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* 基本定时器 定义 */

/* TIMX 中断定义 
 * 默认是针对TIM6/TIM7
 * 注意: 通过修改这4个宏定义,可以支持TIM1~TIM8任意一个定时器.
 */
 
#define BTIM_TIM3_INT                       TIM3
#define BTIM_TIM3_INT_IRQn                  TIM3_IRQn
#define BTIM_TIM3_INT_IRQHandler            TIM3_IRQHandler
#define BTIM_TIM3_INT_CLK_ENABLE()          do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)   /* TIM3 时钟使能 */

/******************************************************************************************/

typedef struct BasicTimer BasicTimer;

/**
 * @brief 基础定时器对象操作表。
 * @details 当前默认用于 FreeRTOS 运行时间统计，也可复用为轻量周期计数器。
 */
typedef struct
{
    void (*init)(BasicTimer *self, uint16_t arr, uint16_t psc);                         /**< 初始化定时器自动重装值和预分频。 */
    void (*reset_runtime_stats)(BasicTimer *self);                                      /**< 清零运行时间统计计数。 */
    void (*handle_irq)(BasicTimer *self);                                               /**< 中断服务入口转发到 HAL。 */
    void (*handle_period_elapsed)(BasicTimer *self, TIM_HandleTypeDef *htim);           /**< HAL 周期到达回调处理。 */
} BasicTimerOps;

/**
 * @brief 基础定时器对象。
 * @note runtime_ticks 指向外部统计变量，使定时器驱动不直接依赖 FreeRTOS 内核实现。
 */
struct BasicTimer
{
    TIM_HandleTypeDef *handle; /**< HAL TIM 句柄。 */
    TIM_TypeDef *instance;     /**< 具体 TIM 外设实例。 */
    IRQn_Type irq;             /**< NVIC 中断号。 */
    uint32_t *runtime_ticks;   /**< 运行时间统计累加变量。 */
    const BasicTimerOps *ops;  /**< 基础定时器方法表。 */
};

extern BasicTimer g_btim3_runtime_timer;

void btim_tim3_int_init(uint16_t arr, uint16_t psc);    /* 基本定时器 定时中断初始化函数 */
void ConfigureTimeForRunTimeStats(void);                /* 配置定时器以用于运行时间统计 */
#endif

















