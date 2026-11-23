/**
 * @file Drivers/BSP/LED/led.c
 * @brief 板载 LED GPIO 控制对象模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/LED/led.h"

/* =========================================================================
 * 内部私有方法前置声明
 * ========================================================================= */
static void LedDevice_Init(LedDevice *self);
static void LedDevice_Write(LedDevice *self, uint8_t off);
static void LedDevice_Toggle(LedDevice *self);

/* =========================================================================
 * 虚函数表定义 (V-Table)
 * ========================================================================= */
/** @brief LED 设备操作接口绑定表 */
static const LedDeviceOps s_led_device_ops =
{
    LedDevice_Init,
    LedDevice_Write,
    LedDevice_Toggle
};

/* =========================================================================
 * 全局对象实例化
 * ========================================================================= */
/** @brief LED1 ~ LED4 硬件设备实例，绑定具体的 GPIO 端口、引脚及操作接口 */
LedDevice g_led1 = {LED1_GPIO_PORT, LED1_GPIO_PIN, &s_led_device_ops};
LedDevice g_led2 = {LED2_GPIO_PORT, LED2_GPIO_PIN, &s_led_device_ops};
LedDevice g_led3 = {LED3_GPIO_PORT, LED3_GPIO_PIN, &s_led_device_ops};
LedDevice g_led4 = {LED4_GPIO_PORT, LED4_GPIO_PIN, &s_led_device_ops};

/* =========================================================================
 * 对象私有方法实现
 * ========================================================================= */

/**
 * @brief 初始化单个 LED 对象的 GPIO 硬件配置
 * @param self 指向待初始化的 LED 设备实例
 */
static void LedDevice_Init(LedDevice *self)
{
    GPIO_InitTypeDef gpio_init_struct;

    if (self == NULL)
    {
        return;
    }

    /* 配置为推挽输出、上拉、高速 */
    gpio_init_struct.Pin = self->pin;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(self->port, &gpio_init_struct);
}

/**
 * @brief 控制单个 LED 对象的亮灭状态
 * @param self 指向被控制的 LED 设备实例
 * @param off  控制指令 (非0: 熄灭 / 输出高电平; 0: 点亮 / 输出低电平)
 * @note 此处逻辑假设硬件电路为低电平点亮 (Active-Low)
 */
static void LedDevice_Write(LedDevice *self, uint8_t off)
{
    if (self == NULL)
    {
        return;
    }

    HAL_GPIO_WritePin(self->port, self->pin, (off != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief 翻转单个 LED 对象的状态
 * @param self 指向待翻转的 LED 设备实例
 */
static void LedDevice_Toggle(LedDevice *self)
{
    if (self == NULL)
    {
        return;
    }

    HAL_GPIO_TogglePin(self->port, self->pin);
}

/* =========================================================================
 * 暴露给外部业务层的公共 API (兼容层/包装器)
 * ========================================================================= */

/**
 * @brief 统筹初始化板载的所有 LED
 * @details 
 * 1. 开启所有关联 GPIO 端口的时钟。
 * 2. 依次调用各 LED 对象的 init 方法进行引脚配置。
 * 3. 默认将所有 LED 初始状态设置为熄灭。
 */
void led_init(void)
{
    /* 1. 使能各 LED 所在的 GPIO 端口时钟 (宏定义通常在 led.h 中) */
    LED1_GPIO_CLK_ENABLE();
    LED2_GPIO_CLK_ENABLE();
    LED3_GPIO_CLK_ENABLE();
    LED4_GPIO_CLK_ENABLE();

    /* 2. 面向对象式初始化硬件引脚 */
    g_led1.ops->init(&g_led1);
    g_led2.ops->init(&g_led2);
    g_led3.ops->init(&g_led3);
    g_led4.ops->init(&g_led4);

    /* 3. 初始状态置为熄灭 (传入 1U，输出高电平) */
    g_led1.ops->write(&g_led1, 1U);
    g_led2.ops->write(&g_led2, 1U);
    g_led3.ops->write(&g_led3, 1U);
    g_led4.ops->write(&g_led4, 1U);
}

