/**
 * @file Drivers/BSP/EXTI/exti.c
 * @brief 外部中断初始化与回波中断转发模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./SYSTEM/sys/sys.h"
#include "./BSP/KEY/key.h"
#include "./BSP/EXTI/exti.h"
#include "./BSP/TIMER/ultrasonic_timer.h"
#include <stdio.h>

static void UltrasonicEchoExti_Init(UltrasonicEchoExti *self);
static void UltrasonicEchoExti_RegisterCallback(UltrasonicEchoExti *self, ultrasonic_echo_callback_t callback);
static void UltrasonicEchoExti_HandleIrq(UltrasonicEchoExti *self, uint16_t gpio_pin);
static void Exti_ConfigKeyLine(GPIO_TypeDef *port, uint16_t pin, IRQn_Type irqn, uint32_t sub_priority);

static const UltrasonicEchoExtiOps s_ultrasonic_echo_exti_ops =
{
    UltrasonicEchoExti_Init,
    UltrasonicEchoExti_RegisterCallback,
    UltrasonicEchoExti_HandleIrq
};

UltrasonicEchoExti g_ultrasonic_echo_exti =
{
    ULTRASONIC_ECHO_GPIO_PORT,
    ULTRASONIC_ECHO_GPIO_PIN,
    ULTRASONIC_ECHO_IRQn,
    NULL,
    &s_ultrasonic_echo_exti_ops
};

void KEY1_INT_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY1_INT_GPIO_PIN);
    __HAL_GPIO_EXTI_CLEAR_IT(KEY1_INT_GPIO_PIN);
}

void KEY2_INT_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY2_INT_GPIO_PIN);
    __HAL_GPIO_EXTI_CLEAR_IT(KEY2_INT_GPIO_PIN);
}

void KEY3_INT_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY3_INT_GPIO_PIN);
    __HAL_GPIO_EXTI_CLEAR_IT(KEY3_INT_GPIO_PIN);
}

void WKUP_INT_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(ULTRASONIC_ECHO_GPIO_PIN) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(ULTRASONIC_ECHO_GPIO_PIN);
        g_ultrasonic_echo_exti.ops->handle_irq(&g_ultrasonic_echo_exti, ULTRASONIC_ECHO_GPIO_PIN);
    }

    if (__HAL_GPIO_EXTI_GET_IT(WKUP_INT_GPIO_PIN) != RESET)
    {
        HAL_GPIO_EXTI_IRQHandler(WKUP_INT_GPIO_PIN);
        __HAL_GPIO_EXTI_CLEAR_IT(WKUP_INT_GPIO_PIN);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if ((GPIO_Pin == KEY2_INT_GPIO_PIN) && (KEY2 == 0))
    {
        printf("KEY2 interrupt\r\n");
    }
}

void extix_init(void)
{
    key_init();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    Exti_ConfigKeyLine(KEY1_INT_GPIO_PORT, KEY1_INT_GPIO_PIN, KEY1_INT_IRQn, 1U);
    Exti_ConfigKeyLine(KEY2_INT_GPIO_PORT, KEY2_INT_GPIO_PIN, KEY2_INT_IRQn, 2U);
    Exti_ConfigKeyLine(KEY3_INT_GPIO_PORT, KEY3_INT_GPIO_PIN, KEY3_INT_IRQn, 3U);
}

static void Exti_ConfigKeyLine(GPIO_TypeDef *port, uint16_t pin, IRQn_Type irqn, uint32_t sub_priority)
{
    GPIO_InitTypeDef gpio_init_struct;

    gpio_init_struct.Pin = pin;
    gpio_init_struct.Mode = GPIO_MODE_IT_FALLING;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &gpio_init_struct);

    HAL_NVIC_SetPriority(irqn, 5, sub_priority);
    HAL_NVIC_EnableIRQ(irqn);
}

static void UltrasonicEchoExti_Init(UltrasonicEchoExti *self)
{
    GPIO_InitTypeDef gpio_init_struct;

    if (self == NULL)
    {
        return;
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    gpio_init_struct.Pin = self->pin;
    gpio_init_struct.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(self->port, &gpio_init_struct);

    HAL_NVIC_SetPriority(self->irqn, 5, 0);
    HAL_NVIC_EnableIRQ(self->irqn);
}

static void UltrasonicEchoExti_RegisterCallback(UltrasonicEchoExti *self, ultrasonic_echo_callback_t callback)
{
    if (self == NULL)
    {
        return;
    }

    self->callback = callback;
}

static void UltrasonicEchoExti_HandleIrq(UltrasonicEchoExti *self, uint16_t gpio_pin)
{
    if ((self == NULL) || (gpio_pin != self->pin) || (self->callback == NULL))
    {
        return;
    }

    self->callback(ultrasonic_timebase_get_us(),
                   HAL_GPIO_ReadPin(self->port, self->pin));
}

void ultrasonic_echo_exti_init(void)
{
    g_ultrasonic_echo_exti.ops->init(&g_ultrasonic_echo_exti);
}

void ultrasonic_echo_exti_register_callback(ultrasonic_echo_callback_t callback)
{
    g_ultrasonic_echo_exti.ops->register_callback(&g_ultrasonic_echo_exti, callback);
}

void ultrasonic_echo_exti_irq_handler(uint16_t gpio_pin)
{
    g_ultrasonic_echo_exti.ops->handle_irq(&g_ultrasonic_echo_exti, gpio_pin);
}
