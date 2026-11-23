/**
 * @file Drivers/SYSTEM/usart/usart.h
 * @brief USART1 调试串口对象与 printf 重定向模块。
 * @details
 * - 属于系统基础服务层，封装时钟、延时、串口或芯片级公共能力。
 * - 上层业务依赖稳定接口，不复制 HAL 初始化细节。
 * - 修改系统级参数时需要同步考虑 FreeRTOS 调度和中断优先级约束。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef _USART_H
#define _USART_H

#include "stdio.h"
#include "./SYSTEM/sys/sys.h"

/*******************************************************************************************************/
/* 引脚 和 串口 定义 
 * 默认是针对USART1的.
 * 注意: 通过修改这12个宏定义,可以支持USART1~UART7任意一个串口.
 */

#define USART_TX_GPIO_PORT              GPIOA
#define USART_TX_GPIO_PIN               GPIO_PIN_9
#define USART_TX_GPIO_AF                GPIO_AF7_USART1
#define USART_TX_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* 发送引脚时钟使能 */

#define USART_RX_GPIO_PORT              GPIOA
#define USART_RX_GPIO_PIN               GPIO_PIN_10
#define USART_RX_GPIO_AF                GPIO_AF7_USART1
#define USART_RX_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* 接收引脚时钟使能 */

#define USART_UX                        USART1
#define USART_UX_IRQn                   USART1_IRQn
#define USART_UX_IRQHandler             USART1_IRQHandler
#define USART_UX_CLK_ENABLE()           do{ __HAL_RCC_USART1_CLK_ENABLE(); }while(0)  /* USART1 时钟使能 */

/*******************************************************************************************************/

#define USART_REC_LEN   200                     /* 定义最大接收字节数 200 */
#define USART_EN_RX     1                       /* 使能（1）/禁止（0）串口1接收 */
#define RXBUFFERSIZE    1                       /* 缓存大小 */

extern UART_HandleTypeDef g_uart1_handle;       /* UART句柄 */

extern uint8_t  g_usart_rx_buf[USART_REC_LEN];  /* 接收缓冲,最大USART_REC_LEN个字节.末字节为换行符 */
extern uint16_t g_usart_rx_sta;                 /* 接收状态标记 */
extern uint8_t g_rx_buffer[RXBUFFERSIZE];       /* HAL库USART接收Buffer */

typedef struct UsartPort UsartPort;

/**
 * @brief USART 端口对象操作表。
 * @details 串口初始化、接收完成回调、中断处理和重新挂接接收缓冲统一由对象管理。
 */
typedef struct
{
    void (*init)(UsartPort *self, uint32_t baudrate);                         /**< 初始化 UART 外设和 NVIC。 */
    void (*handle_rx_complete)(UsartPort *self, UART_HandleTypeDef *huart);   /**< HAL 接收完成回调入口。 */
    void (*handle_irq)(UsartPort *self);                                      /**< USART IRQHandler 转发入口。 */
    void (*restart_rx)(UsartPort *self);                                      /**< 重新启动单字节中断接收。 */
} UsartPortOps;

/**
 * @brief USART 端口对象。
 * @note 当前默认实例绑定 USART1；如需新增串口，可复用同一结构实例化第二个对象。
 */
struct UsartPort
{
    UART_HandleTypeDef *handle; /**< HAL UART 句柄。 */
    USART_TypeDef *instance;    /**< USART 外设基地址。 */
    IRQn_Type irq;              /**< 对应 NVIC 中断号。 */
    uint8_t *rx_buf;            /**< 行接收缓冲区。 */
    uint16_t *rx_sta;           /**< 行接收状态字，包含完成和长度信息。 */
    uint8_t *rx_byte;           /**< HAL 单字节接收缓冲。 */
    uint16_t rx_buf_len;        /**< 行接收缓冲区容量。 */
    uint16_t rx_byte_len;       /**< 单次 HAL 接收长度。 */
    const UsartPortOps *ops;    /**< 串口端口方法表。 */
};

extern UsartPort g_usart1_port;

void usart_init(uint32_t baudrate);             /* 串口初始化函数 */

#endif







