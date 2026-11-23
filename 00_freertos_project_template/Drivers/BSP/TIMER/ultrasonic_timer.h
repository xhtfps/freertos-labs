/**
 * @file Drivers/BSP/TIMER/ultrasonic_timer.h
 * @brief 超声定时器兼容 API 聚合层。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __ULTRASONIC_TIMER_H
#define __ULTRASONIC_TIMER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* 超声波时序与参数配置宏定义                                                 */
/* ========================================================================== */

#define ULTRASONIC_PWM_FREQ_HZ         40000U      /* 超声波发射频率：40kHz（市面上绝大多数探头的标准频率） */
#define ULTRASONIC_TIM_CLK_HZ          84000000U   /* 定时器 TIM4 的输入时钟频率：84MHz（APB1时钟倍频后的值） */

/* PWM 自动重装载值（Period Ticks）：84,000,000 / 40,000 = 2100 滴答。意味着计数器数到 2100 为一个周期 */
#define ULTRASONIC_PWM_PERIOD_TICKS    (ULTRASONIC_TIM_CLK_HZ / ULTRASONIC_PWM_FREQ_HZ)

/* PWM 占空比比较值（Pulse Ticks）：2100 / 2 = 1050 滴答，即 50% 占空比方波 */
#define ULTRASONIC_PWM_PULSE_TICKS     (ULTRASONIC_PWM_PERIOD_TICKS / 2U)

#define ULTRASONIC_BURST_CYCLES        8U          /* 单词探测发射的脉冲个数（Burst）：连续发送 8 个波形 */
#define ULTRASONIC_RX_RECOVER_US       15U         /* 接收电路恢复/盲区等待时间：15微秒 */

/* ========================================================================== */
/* 定时器及 PWM 发射引脚硬件映射 (使用 TIM4)                                    */
/* ========================================================================== */

#define ULTRASONIC_PWM_TIM             TIM4        /* 使用 TIM4 作为超声波 PWM 发生器 */
#define ULTRASONIC_PWM_IRQn            TIM4_IRQn   /* TIM4 中断源 */
#define ULTRASONIC_PWM_IRQHandler      TIM4_IRQHandler /* TIM4 中断服务函数名映射 */

#define ULTRASONIC_PWM_GPIO_PORT       GPIOD       /* PWM 引脚所在的 GPIO 端口为 GPIOD */
#define ULTRASONIC_PWM_CH1_PIN         GPIO_PIN_12 /* TIM4_CH1 对应 PD12（用于差分驱动正相） */
#define ULTRASONIC_PWM_CH2_PIN         GPIO_PIN_13 /* TIM4_CH2 对应 PD13（用于差分驱动反相） */
#define ULTRASONIC_PWM_GPIO_AF         GPIO_AF2_TIM4 /* TIM4 的引脚复用功能映射：AF2 */

/* ========================================================================== */
/* 收发控制/比较器控制引脚及开关宏定义 (使用 GPIOC)                              */
/* ========================================================================== */

#define ULTRASONIC_RX_RECOVER_US       15U         /* 接收电路恢复/盲区等待时间：15微秒 */

#define ULTRASONIC_RX_CMP_CTRL_PORT    GPIOC       /* 接收控制引脚所在端口 */
#define ULTRASONIC_RX_CMP_CTRL_PIN     GPIO_PIN_7  /* PC7: 控制接收电路/比较器的使能开关 */

#define ULTRASONIC_TX_CMP_CTRL_PORT    GPIOC       /* 发射控制引脚所在端口 */
#define ULTRASONIC_TX_CMP_CTRL_PIN     GPIO_PIN_6  /* PC6: 控制发射硬件驱动/比较器的使能开关 */

/**
 * @brief 开启/关闭接收和发射电路的控制宏
 * @note  根据硬件设计，这里采用低电平有效（RESET 开启，SET 关闭）
 */
/* 开启接收电路：PC7 拉低 */
#define ULTRASONIC_RX_CMP_ON()         HAL_GPIO_WritePin(ULTRASONIC_RX_CMP_CTRL_PORT, ULTRASONIC_RX_CMP_CTRL_PIN, GPIO_PIN_RESET)
/* 关闭接收电路：PC7 拉高 */
#define ULTRASONIC_RX_CMP_OFF()        HAL_GPIO_WritePin(ULTRASONIC_RX_CMP_CTRL_PORT, ULTRASONIC_RX_CMP_CTRL_PIN, GPIO_PIN_SET)
/* 开启发射电路：PC6 拉低 */
#define ULTRASONIC_TX_CMP_ON()         HAL_GPIO_WritePin(ULTRASONIC_TX_CMP_CTRL_PORT, ULTRASONIC_TX_CMP_CTRL_PIN, GPIO_PIN_RESET)
/* 关闭发射电路：PC6 拉高 */
#define ULTRASONIC_TX_CMP_OFF()        HAL_GPIO_WritePin(ULTRASONIC_TX_CMP_CTRL_PORT, ULTRASONIC_TX_CMP_CTRL_PIN, GPIO_PIN_SET)

/* ========================================================================== */
/* 外部全局变量声明                                                            */
/* ========================================================================== */

extern TIM_HandleTypeDef g_ultrasonic_pwm_tim; /* 声明在 .c 中定义的 TIM4 句柄，方便外部调用 */

/* ========================================================================== */
/* 函数声明                                                                   */
/* ========================================================================== */

/* --- 计时基础（Timebase）相关函数 --- */
void ultrasonic_timebase_init(void);      /* 初始化 1MHz 测距基础计时器（通常是 TIM5） */
void ultrasonic_timebase_reset(void);     /* 将测距计时器的计数值清零（发射波形完毕时使用） */
uint32_t ultrasonic_timebase_get_us(void); /* 获取自重置以来经过的微秒数（us） */

/* --- PWM 发射控制相关函数 --- */
void Ultrasonic_PWM_Init(void);           /* 初始化 PWM 定时器硬件、中断及收发 GPIO */
void Ultrasonic_PWM_OutputEnable(void);   /* 持续开启 PWM 输出（常发模式，不受脉冲数限制） */
void Ultrasonic_PWM_OutputDisable(void);  /* 强行关闭 PWM 输出，并切回接收状态 */
void Ultrasonic_FireBurst(void);          /* 精准单次发射 8 个周期的脉冲串，发完自动停止并转为接收 */
uint8_t ultrasonic_pwm_is_active(void);   /* 获取当前 PWM 发射是否活跃（1: 正在发射/使能；0: 空闲） */

#ifdef __cplusplus
}
#endif

#endif /* __ULTRASONIC_TIMER_H */
