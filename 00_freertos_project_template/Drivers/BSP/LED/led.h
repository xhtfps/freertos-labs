/**
 * @file Drivers/BSP/LED/led.h
 * @brief 板载 LED GPIO 控制对象模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __LED_H
#define __LED_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* 引脚 定义 */

#define LED1_GPIO_PORT                  GPIOB
#define LED1_GPIO_PIN                   GPIO_PIN_7
#define LED1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)             /* PB口时钟使能 */

#define LED2_GPIO_PORT                  GPIOB
#define LED2_GPIO_PIN                   GPIO_PIN_8
#define LED2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)             /* PB口时钟使能 */

#define LED3_GPIO_PORT                  GPIOD
#define LED3_GPIO_PIN                   GPIO_PIN_2
#define LED3_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)             /* PD口时钟使能 */

#define LED4_GPIO_PORT                  GPIOB
#define LED4_GPIO_PIN                   GPIO_PIN_1
#define LED4_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)             /* PB口时钟使能 */

/******************************************************************************************/

/* LED端口定义 */
#define LED1(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)       

#define LED2(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)      

#define LED3(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)       

#define LED4(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED4_GPIO_PORT, LED4_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED4_GPIO_PORT, LED4_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)       

/* LED取反定义 */
#define LED1_TOGGLE()    do{ HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN); }while(0)       /* LED1 = !LED1 */
#define LED2_TOGGLE()    do{ HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_GPIO_PIN); }while(0)       /* LED2 = !LED2 */
#define LED3_TOGGLE()    do{ HAL_GPIO_TogglePin(LED3_GPIO_PORT, LED3_GPIO_PIN); }while(0)       /* LED3 = !LED3 */
#define LED4_TOGGLE()    do{ HAL_GPIO_TogglePin(LED4_GPIO_PORT, LED4_GPIO_PIN); }while(0)       /* LED4 = !LED4 */

/******************************************************************************************/
/* LED 对象接口 */

typedef struct LedDevice LedDevice;

/**
 * @brief LED 设备对象操作表。
 */
typedef struct
{
    void (*init)(LedDevice *self);          /**< 初始化 LED 对应 GPIO。 */
    void (*write)(LedDevice *self, uint8_t off); /**< 写入 LED 状态，off 遵循原驱动的亮灭语义。 */
    void (*toggle)(LedDevice *self);        /**< 翻转 LED 当前输出状态。 */
} LedDeviceOps;

/**
 * @brief 单个 LED 设备对象。
 */
struct LedDevice
{
    GPIO_TypeDef *port;       /**< LED GPIO 端口。 */
    uint16_t pin;             /**< LED GPIO 引脚。 */
    const LedDeviceOps *ops;  /**< LED 方法表。 */
};

extern LedDevice g_led1;
extern LedDevice g_led2;
extern LedDevice g_led3;
extern LedDevice g_led4;

/******************************************************************************************/
/* 外部接口函数*/
void led_init(void);                                                                           /* 初始化 */

#endif
