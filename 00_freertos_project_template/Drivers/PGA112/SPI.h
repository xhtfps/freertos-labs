/**
 * @file Drivers/PGA112/SPI.h
 * @brief PGA112 旧软件 SPI 兼容接口模块。
 * @details
 * - 属于 PGA112 增益控制链路，分离设备语义和软件 SPI 总线时序。
 * - 测量服务只关心增益码和通道码，底层帧格式与 GPIO 翻转保留在驱动内部。
 * - 旧 API 保留为兼容层，新代码优先使用对象接口。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __PGA112_SPI_H
#define __PGA112_SPI_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* 软件模拟 SPI 引脚定义（使用 GPIOE）                                         */
/* ========================================================================== */
/* 注：PGA112 采用三线 SPI 接口，没有独立的 MISO 和 MOSI，而是合并为单根双向数据线 DIO */

#define USER_SPI_PORT               GPIOE       /* 模拟 SPI 占用的 GPIO 端口组为 GPIOE */
#define USER_SPI_CS_PIN             GPIO_PIN_0  /* PE0 映射为 CS 片选引脚（低电平有效） */
#define USER_SPI_SCK_PIN            GPIO_PIN_2  /* PE2 映射为 SCK 时钟引脚 */
#define USER_SPI_DIO_PIN            GPIO_PIN_4  /* PE4 映射为 DIO 双向数据引脚（兼顾发送与接收） */

/* ========================================================================== */
/* 底层 GPIO 快速操作宏定义（通过 HAL 库封装）                                  */
/* ========================================================================== */

/* --- 片选引脚（CS）控制 --- */
#define USER_SPI_CS_LOW()            HAL_GPIO_WritePin(USER_SPI_PORT, USER_SPI_CS_PIN, GPIO_PIN_RESET) /* 拉低CS：选中 PGA112 */
#define USER_SPI_CS_HIGH()           HAL_GPIO_WritePin(USER_SPI_PORT, USER_SPI_CS_PIN, GPIO_PIN_SET)   /* 拉高CS：释放 PGA112 */

/* --- 时钟引脚（SCK）控制 --- */
#define USER_SPI_SCK_LOW()           HAL_GPIO_WritePin(USER_SPI_PORT, USER_SPI_SCK_PIN, GPIO_PIN_RESET) /* 拉低时钟线 */
#define USER_SPI_SCK_HIGH()          HAL_GPIO_WritePin(USER_SPI_PORT, USER_SPI_SCK_PIN, GPIO_PIN_SET)   /* 拉高时钟线 */

/* --- 数据引脚（DIO）控制与读取 --- */
#define USER_SPI_DIO_LOW()           HAL_GPIO_WritePin(USER_SPI_PORT, USER_SPI_DIO_PIN, GPIO_PIN_RESET) /* 驱动数据线输出低电平（写0） */
#define USER_SPI_DIO_HIGH()          HAL_GPIO_WritePin(USER_SPI_PORT, USER_SPI_DIO_PIN, GPIO_PIN_SET)   /* 驱动数据线输出高电平（写1） */
#define USER_SPI_READ_DIO()          HAL_GPIO_ReadPin(USER_SPI_PORT, USER_SPI_DIO_PIN)                  /* 读取数据线的当前电平状态（读状态） */

/* ========================================================================== */
/* 外部可调用函数声明                                                         */
/* ========================================================================== */

void User_SPI_Init(void);          /* 初始化整个模拟 SPI 模块 */
void SPI_GPIO_Init(uint8_t mode);  /* 初始化底层 GPIO 硬件引脚为输入/输出状态 */

void User_SPI_SetDioOutput(void);  /* 切换 DIO 引脚为 推挽输出 状态（用于向芯片发送指令/数据） */
void User_SPI_SetDioInput(void);   /* 切换 DIO 引脚为 浮空/上拉输入 状态（用于从芯片读取数据） */

void User_SPI_StartFrame(void);    /* 开启一次 SPI 传输流（包含切换输出、拉低 CS 建立延时） */
void User_SPI_EndFrame(void);      /* 结束一次 SPI 传输流（包含保持延时、拉高 CS 并释放总线） */

/**
 * @brief  非标/可变长度的数据移位发送函数
 * @param  tx_data: 待发数据
 * @param  length:  发送的总比特位数（1~32）
 * @param  pha:     时钟相位控制（0或1）
 */
void User_SPI_SendData(uint32_t tx_data, uint8_t length, uint8_t pha);

/**
 * @brief  标准 16 位全双工/半双工读写函数
 * @param  tx_data: 准备发往 PGA112 的 16 位数据
 * @retval 伴随传输从 PGA112 读回的 16 位数据
 */
uint16_t User_SPI_Transfer16(uint16_t tx_data);

void User_SPI_Write16(uint16_t tx_data); /* 快捷写 16 位控制字函数（内部自带完整的片选开启与关闭流程） */

#ifdef __cplusplus
}
#endif

#endif /* __PGA112_SPI_H */
