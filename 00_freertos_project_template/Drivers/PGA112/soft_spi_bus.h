/**
 * @file Drivers/PGA112/soft_spi_bus.h
 * @brief PGA112 专用软件 SPI 总线对象模块。
 * @details
 * - 属于 PGA112 增益控制链路，分离设备语义和软件 SPI 总线时序。
 * - 测量服务只关心增益码和通道码，底层帧格式与 GPIO 翻转保留在驱动内部。
 * - 旧 API 保留为兼容层，新代码优先使用对象接口。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __SOFT_SPI_BUS_H
#define __SOFT_SPI_BUS_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SoftSpiBus SoftSpiBus;

/**
 * @brief 软件 SPI 总线的操作表。
 * @details
 * 该表把位级 GPIO 操作封装为“总线方法”，设备层只需要按帧读写，
 * 不直接关心 DIO 方向切换、片选时序和时钟相位细节。
 */
typedef struct
{
    void (*init)(SoftSpiBus *self, uint8_t mode);                                      /**< 初始化 CS/SCK/DIO 引脚方向与默认电平。 */
    void (*set_dio_output)(SoftSpiBus *self);                                          /**< 将半双工 DIO 线切换为推挽输出。 */
    void (*set_dio_input)(SoftSpiBus *self);                                           /**< 将半双工 DIO 线切换为输入，用于读回数据。 */
    void (*start_frame)(SoftSpiBus *self);                                             /**< 拉低片选并建立一帧传输前的总线条件。 */
    void (*end_frame)(SoftSpiBus *self);                                               /**< 结束传输、拉高片选并释放总线。 */
    void (*send_data)(SoftSpiBus *self, uint32_t tx_data, uint8_t length, uint8_t pha); /**< 按指定 bit 数和相位发送原始数据。 */
    uint16_t (*transfer16)(SoftSpiBus *self, uint16_t tx_data);                        /**< 发送 16 位数据并同步读取 16 位返回值。 */
    void (*write16)(SoftSpiBus *self, uint16_t tx_data);                               /**< 发送一帧 16 位 PGA112 控制字。 */
} SoftSpiBusOps;

/**
 * @brief 软件 SPI 总线对象。
 * @note 该对象只描述一条具体物理总线，不保存 PGA112 的增益或通道语义。
 */
struct SoftSpiBus
{
    GPIO_TypeDef *port;       /**< CS/SCK/DIO 共享的 GPIO 端口。 */
    uint16_t cs_pin;          /**< 片选引脚，低电平表示本帧选中目标器件。 */
    uint16_t sck_pin;         /**< 软件 SPI 时钟引脚。 */
    uint16_t dio_pin;         /**< 半双工数据引脚，读写时需要切换方向。 */
    const SoftSpiBusOps *ops; /**< 对象方法表，模拟 C++ 成员函数调用。 */
};

extern SoftSpiBus g_pga112_spi_bus;

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_SPI_BUS_H */
