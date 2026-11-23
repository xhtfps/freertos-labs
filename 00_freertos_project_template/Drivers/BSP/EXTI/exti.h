/**
 * @file Drivers/BSP/EXTI/exti.h
 * @brief 外部中断初始化与回波中断转发模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __EXTI_H
#define __EXTI_H

#include "./SYSTEM/sys/sys.h"
#include <stdint.h>


/******************************************************************************************/
/* 引脚 和 中断编号 & 中断服务函数 定义 */ 

#define KEY1_INT_GPIO_PORT              GPIOC
#define KEY1_INT_GPIO_PIN               GPIO_PIN_1
#define KEY1_INT_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */
#define KEY1_INT_IRQn                   EXTI1_IRQn
#define KEY1_INT_IRQHandler             EXTI1_IRQHandler

#define KEY2_INT_GPIO_PORT              GPIOC
#define KEY2_INT_GPIO_PIN               GPIO_PIN_2
#define KEY2_INT_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */
#define KEY2_INT_IRQn                   EXTI2_IRQn
#define KEY2_INT_IRQHandler             EXTI2_IRQHandler

#define KEY3_INT_GPIO_PORT              GPIOC
#define KEY3_INT_GPIO_PIN               GPIO_PIN_3
#define KEY3_INT_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */
#define KEY3_INT_IRQn                   EXTI3_IRQn
#define KEY3_INT_IRQHandler             EXTI3_IRQHandler

#define WKUP_INT_GPIO_PORT              GPIOA
#define WKUP_INT_GPIO_PIN               GPIO_PIN_0
#define WKUP_INT_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */
#define WKUP_INT_IRQn                   EXTI0_IRQn
#define WKUP_INT_IRQHandler             EXTI0_IRQHandler

#define ULTRASONIC_ECHO_GPIO_PORT       GPIOC
#define ULTRASONIC_ECHO_GPIO_PIN        GPIO_PIN_0
#define ULTRASONIC_ECHO_IRQn            EXTI0_IRQn

/******************************************************************************************/


void extix_init(void);  /* 外部中断初始化 */
typedef void (*ultrasonic_echo_callback_t)(uint32_t now_us, GPIO_PinState level);

typedef struct UltrasonicEchoExti UltrasonicEchoExti;

/**
 * @brief 超声回波 EXTI 对象操作表。
 * @details EXTI 层只负责读取边沿电平和时间戳，再把事件转发给测量服务回调。
 */
typedef struct
{
    void (*init)(UltrasonicEchoExti *self);                                           /**< 配置回波 GPIO 和 EXTI 中断。 */
    void (*register_callback)(UltrasonicEchoExti *self, ultrasonic_echo_callback_t callback); /**< 注册测量服务回调。 */
    void (*handle_irq)(UltrasonicEchoExti *self, uint16_t gpio_pin);                  /**< HAL EXTI 回调中的分发入口。 */
} UltrasonicEchoExtiOps;

/**
 * @brief 超声回波 EXTI 对象。
 */
struct UltrasonicEchoExti
{
    GPIO_TypeDef *port;                    /**< 回波输入 GPIO 端口。 */
    uint16_t pin;                          /**< 回波输入 GPIO 引脚。 */
    IRQn_Type irqn;                        /**< EXTI 中断号。 */
    ultrasonic_echo_callback_t callback;   /**< 注册到测量服务的边沿事件回调。 */
    const UltrasonicEchoExtiOps *ops;      /**< EXTI 方法表。 */
};

extern UltrasonicEchoExti g_ultrasonic_echo_exti;

void ultrasonic_echo_exti_init(void);
void ultrasonic_echo_exti_register_callback(ultrasonic_echo_callback_t callback);
void ultrasonic_echo_exti_irq_handler(uint16_t gpio_pin);

#endif

























