/**
 * @file Drivers/BSP/KEY/key.h
 * @brief 板载按键扫描与去抖对象模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __KEY_H
#define __KEY_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* 引脚 定义 */

#define KEY1_GPIO_PORT                  GPIOC
#define KEY1_GPIO_PIN                   GPIO_PIN_1
#define KEY1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */

#define KEY2_GPIO_PORT                  GPIOC
#define KEY2_GPIO_PIN                   GPIO_PIN_2
#define KEY2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */

#define KEY3_GPIO_PORT                  GPIOC
#define KEY3_GPIO_PIN                   GPIO_PIN_3
#define KEY3_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC口时钟使能 */

#define WKUP_GPIO_PORT                  GPIOA
#define WKUP_GPIO_PIN                   GPIO_PIN_0
#define WKUP_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

/******************************************************************************************/

#define KEY1        HAL_GPIO_ReadPin(KEY1_GPIO_PORT, KEY1_GPIO_PIN)     /* 读取KEY1引脚 */
#define KEY2        HAL_GPIO_ReadPin(KEY2_GPIO_PORT, KEY2_GPIO_PIN)     /* 读取KEY2引脚 */
#define KEY3        HAL_GPIO_ReadPin(KEY3_GPIO_PORT, KEY3_GPIO_PIN)     /* 读取KEY3引脚 */
#define WK_UP       HAL_GPIO_ReadPin(WKUP_GPIO_PORT, WKUP_GPIO_PIN)     /* 读取WKUP引脚 */


#define KEY1_PRES    1              /* KEY1按下 */
#define KEY2_PRES    2              /* KEY2按下 */
#define KEY3_PRES    3              /* KEY3按下 */
#define WKUP_PRES    4              /* KEY_UP按下(即WK_UP) */

typedef struct KeyDevice KeyDevice;
typedef struct KeyScanner KeyScanner;

/**
 * @brief 单个按键对象操作表。
 */
typedef struct
{
    void (*init)(KeyDevice *self);       /**< 初始化按键 GPIO。 */
    uint8_t (*is_pressed)(KeyDevice *self); /**< 读取按键是否处于有效按下电平。 */
} KeyDeviceOps;

/**
 * @brief 单个按键设备对象。
 */
struct KeyDevice
{
    GPIO_TypeDef *port;          /**< 按键 GPIO 端口。 */
    uint16_t pin;                /**< 按键 GPIO 引脚。 */
    GPIO_PinState active_level;  /**< 判定为按下的有效电平。 */
    uint8_t value;               /**< 对外返回的键值。 */
    uint32_t pull;               /**< GPIO 上拉/下拉配置。 */
    const KeyDeviceOps *ops;     /**< 单键方法表。 */
};

/**
 * @brief 多按键扫描器操作表。
 */
typedef struct
{
    void (*init)(KeyScanner *self);                 /**< 初始化扫描器管理的所有按键。 */
    uint8_t (*scan)(KeyScanner *self, uint8_t mode); /**< 扫描按键，mode 控制是否支持连按。 */
} KeyScannerOps;

/**
 * @brief 多按键扫描器对象。
 */
struct KeyScanner
{
    KeyDevice *keys;            /**< 被扫描的按键数组。 */
    uint8_t key_count;          /**< 按键数量。 */
    uint8_t key_up;             /**< 松手锁存标志，用于消抖和单次触发。 */
    const KeyScannerOps *ops;   /**< 扫描器方法表。 */
};

extern KeyDevice g_key_devices[4];
extern KeyScanner g_key_scanner;

void key_init(void);                /* 按键初始化函数 */
uint8_t key_scan(uint8_t mode);     /* 按键扫描函数 */

#endif
