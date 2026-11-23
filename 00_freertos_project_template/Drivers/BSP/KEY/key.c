/**
 * @file Drivers/BSP/KEY/key.c
 * @brief 板载按键扫描与去抖对象模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/KEY/key.h"
#include "./SYSTEM/delay/delay.h"

/* =========================================================================
 * 内部私有方法前置声明
 * ========================================================================= */
static void KeyDevice_Init(KeyDevice *self);
static uint8_t KeyDevice_IsPressed(KeyDevice *self);
static void KeyScanner_Init(KeyScanner *self);
static uint8_t KeyScanner_Scan(KeyScanner *self, uint8_t mode);

/* =========================================================================
 * 虚函数表定义 (V-Table)
 * ========================================================================= */
/** @brief 单个按键设备的操作接口绑定表 */
static const KeyDeviceOps s_key_device_ops =
{
    KeyDevice_Init,
    KeyDevice_IsPressed
};

/** @brief 按键扫描器的操作接口绑定表 */
static const KeyScannerOps s_key_scanner_ops =
{
    KeyScanner_Init,
    KeyScanner_Scan
};

/* =========================================================================
 * 全局对象实例化
 * ========================================================================= */
/** * @brief 实例化物理按键数组
 * @details 包含 4 个按键，展示了数据驱动多态：
 * - KEY1~KEY3：常态上拉 (PULLUP)，低电平按下 (RESET)。
 * - WKUP：常态下拉 (PULLDOWN)，高电平按下 (SET)。
 */
KeyDevice g_key_devices[4] =
{
    {KEY1_GPIO_PORT, KEY1_GPIO_PIN, GPIO_PIN_RESET, KEY1_PRES, GPIO_PULLUP, &s_key_device_ops},
    {KEY2_GPIO_PORT, KEY2_GPIO_PIN, GPIO_PIN_RESET, KEY2_PRES, GPIO_PULLUP, &s_key_device_ops},
    {KEY3_GPIO_PORT, KEY3_GPIO_PIN, GPIO_PIN_RESET, KEY3_PRES, GPIO_PULLUP, &s_key_device_ops},
    {WKUP_GPIO_PORT, WKUP_GPIO_PIN, GPIO_PIN_SET,   WKUP_PRES, GPIO_PULLDOWN, &s_key_device_ops}
};

/** @brief 实例化全局唯一的按键扫描器 */
KeyScanner g_key_scanner =
{
    g_key_devices,       // 绑定的物理按键数组
    4U,                  // 按键总数
    1U,                  // 初始状态 key_up (1表示按键处于释放状态)
    &s_key_scanner_ops   // 扫描器操作接口
};

/* =========================================================================
 * KeyDevice 对象私有方法实现
 * ========================================================================= */

/**
 * @brief 初始化单个物理按键的 GPIO 硬件配置
 * @param self 指向待初始化的按键实例
 */
static void KeyDevice_Init(KeyDevice *self)
{
    GPIO_InitTypeDef gpio_init_struct;

    if (self == NULL)
    {
        return;
    }

    /* 根据对象自身的属性 (引脚、上下拉) 动态配置 GPIO */
    gpio_init_struct.Pin = self->pin;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = self->pull;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(self->port, &gpio_init_struct);
}

/**
 * @brief 判断当前单个按键是否处于物理按下状态 (无去抖)
 * @param self 指向待检测的按键实例
 * @return 1: 已按下; 0: 未按下
 */
static uint8_t KeyDevice_IsPressed(KeyDevice *self)
{
    if (self == NULL)
    {
        return 0U;
    }

    /* 通过对比当前引脚电平与预设的 active_level 屏蔽高低电平触发差异 */
    return (HAL_GPIO_ReadPin(self->port, self->pin) == self->active_level) ? 1U : 0U;
}

/* =========================================================================
 * KeyScanner 对象私有方法实现
 * ========================================================================= */

/**
 * @brief 初始化按键扫描器
 * @details 开启相关时钟，并遍历初始化所有挂载的物理按键
 * @param self 指向按键扫描器实例
 */
static void KeyScanner_Init(KeyScanner *self)
{
    uint8_t i;

    if (self == NULL)
    {
        return;
    }

    /* 开启相关外设时钟 (此处仍存在一定的板级硬编码，可通过更高层抽象解耦) */
    KEY1_GPIO_CLK_ENABLE();
    KEY2_GPIO_CLK_ENABLE();
    KEY3_GPIO_CLK_ENABLE();
    WKUP_GPIO_CLK_ENABLE();

    /* 遍历初始化托管的所有物理按键 */
    for (i = 0U; i < self->key_count; i++)
    {
        self->keys[i].ops->init(&self->keys[i]);
    }

    /* 初始化按键状态机为释放状态 */
    self->key_up = 1U;
}

/**
 * @brief 执行按键扫描与软件去抖
 * @param self 指向按键扫描器实例
 * @param mode 0: 单次触发 (按住不放只触发一次); 1: 连续触发 (按住不放可持续触发)
 * @return 触发的按键键值 (定义于头文件如 KEY1_PRES); 0 表示无按键按下
 */
static uint8_t KeyScanner_Scan(KeyScanner *self, uint8_t mode)
{
    uint8_t i;
    uint8_t any_pressed = 0U;
    uint8_t key_value = 0U;

    if (self == NULL)
    {
        return 0U;
    }

    /* 连续触发模式下，强制清除 key_up 锁存标志 */
    if (mode != 0U)
    {
        self->key_up = 1U;
    }

    /* 遍历所有按键，检测是否有物理按下动作 */
    for (i = 0U; i < self->key_count; i++)
    {
        if (self->keys[i].ops->is_pressed(&self->keys[i]) != 0U)
        {
            any_pressed = 1U;
            key_value = self->keys[i].value; /* 注意：若多按键同时按下，后遍历的按键值会覆盖前面的 */
        }
    }

    /* 状态机逻辑：当前必须处于释放允许状态 (key_up == 1)，且有按键按下 */
    if ((self->key_up != 0U) && (any_pressed != 0U))
    {
        delay_ms(10); /* 软件延时消抖 (10ms 滤除机械抖动) */
        
        /* 锁定状态机，防止单次模式下重复触发 */
        self->key_up = 0U; 
        
        return key_value;
    }

    /* 如果没有任何按键按下，重置锁存标志，准备下一次检测 */
    if (any_pressed == 0U)
    {
        self->key_up = 1U;
    }

    return 0U;
}

/* =========================================================================
 * 暴露给外部业务层的公共 API (兼容层/包装器)
 * ========================================================================= */

/**
 * @brief 系统级按键初始化接口
 */
void key_init(void)
{
    g_key_scanner.ops->init(&g_key_scanner);
}

/**
 * @brief 系统级按键扫描接口
 * @param mode 0:不支持连按(单次); 1:支持连按
 * @return 对应按键的宏定义值，0表示无有效按键
 */
uint8_t key_scan(uint8_t mode)
{
    return g_key_scanner.ops->scan(&g_key_scanner, mode);
}
