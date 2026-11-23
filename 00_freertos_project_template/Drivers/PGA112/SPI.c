/**
 * @file Drivers/PGA112/SPI.c
 * @brief PGA112 旧软件 SPI 兼容接口模块。
 * @details
 * - 属于 PGA112 增益控制链路，分离设备语义和软件 SPI 总线时序。
 * - 测量服务只关心增益码和通道码，底层帧格式与 GPIO 翻转保留在驱动内部。
 * - 旧 API 保留为兼容层，新代码优先使用对象接口。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./PGA112/SPI.h"
#include "./PGA112/soft_spi_bus.h"

void User_SPI_Init(void)
{
    g_pga112_spi_bus.ops->init(&g_pga112_spi_bus, 1U);
}

void SPI_GPIO_Init(uint8_t mode)
{
    g_pga112_spi_bus.ops->init(&g_pga112_spi_bus, mode);
}

void User_SPI_SetDioOutput(void)
{
    g_pga112_spi_bus.ops->set_dio_output(&g_pga112_spi_bus);
}

void User_SPI_SetDioInput(void)
{
    g_pga112_spi_bus.ops->set_dio_input(&g_pga112_spi_bus);
}

void User_SPI_StartFrame(void)
{
    g_pga112_spi_bus.ops->start_frame(&g_pga112_spi_bus);
}

void User_SPI_EndFrame(void)
{
    g_pga112_spi_bus.ops->end_frame(&g_pga112_spi_bus);
}

void User_SPI_SendData(uint32_t tx_data, uint8_t length, uint8_t pha)
{
    g_pga112_spi_bus.ops->send_data(&g_pga112_spi_bus, tx_data, length, pha);
}

uint16_t User_SPI_Transfer16(uint16_t tx_data)
{
    return g_pga112_spi_bus.ops->transfer16(&g_pga112_spi_bus, tx_data);
}

void User_SPI_Write16(uint16_t tx_data)
{
    g_pga112_spi_bus.ops->write16(&g_pga112_spi_bus, tx_data);
}
