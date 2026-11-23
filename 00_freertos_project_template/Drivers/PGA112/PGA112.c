/**
 * @file Drivers/PGA112/PGA112.c
 * @brief PGA112 可编程增益放大器设备对象模块。
 * @details
 * - 属于 PGA112 增益控制链路，分离设备语义和软件 SPI 总线时序。
 * - 测量服务只关心增益码和通道码，底层帧格式与 GPIO 翻转保留在驱动内部。
 * - 旧 API 保留为兼容层，新代码优先使用对象接口。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./PGA112/PGA112.h"
#include "./PGA112/soft_spi_bus.h"
#include "./SYSTEM/delay/delay.h"

static void Pga112Device_Init(Pga112Device *self);
static void Pga112Device_Shutdown(Pga112Device *self, FunctionalState new_state);
static void Pga112Device_WriteConfig(Pga112Device *self, uint8_t gain_code, uint8_t channel_code);
static void Pga112Device_SetGainCode(Pga112Device *self, uint8_t gain_code);
static void Pga112Device_SelectChannel(Pga112Device *self, uint8_t channel_code);
static uint8_t Pga112Device_GetGainCode(Pga112Device *self);
static uint8_t Pga112Device_GetChannelCode(Pga112Device *self);
static uint16_t Pga112Device_GetGainValue(Pga112Device *self, uint8_t gain_code);
static uint16_t Pga112Device_BuildWriteWord(uint8_t gain_code, uint8_t channel_code);

static const Pga112DeviceOps s_pga112_device_ops =
{
    Pga112Device_Init,
    Pga112Device_Shutdown,
    Pga112Device_WriteConfig,
    Pga112Device_SetGainCode,
    Pga112Device_SelectChannel,
    Pga112Device_GetGainCode,
    Pga112Device_GetChannelCode,
    Pga112Device_GetGainValue
};

Pga112Device g_pga112_device =
{
    &g_pga112_spi_bus,
    PGA112_DEFAULT_GAIN_CODE,
    PGA112_ULTRASONIC_CHANNEL,
    &s_pga112_device_ops
};

static void Pga112Device_Init(Pga112Device *self)
{
    if ((self == NULL) || (self->bus == NULL))
    {
        return;
    }

    self->bus->ops->init(self->bus, 1U);
    HAL_Delay(2);

    self->ops->shutdown(self, ENABLE);
    HAL_Delay(1);
    self->ops->shutdown(self, DISABLE);
    HAL_Delay(1);

    self->ops->write_config(self, self->gain_code, self->channel_code);
}

static void Pga112Device_Shutdown(Pga112Device *self, FunctionalState new_state)
{
    if ((self == NULL) || (self->bus == NULL))
    {
        return;
    }

    self->bus->ops->write16(self->bus, (new_state != DISABLE) ? PGA112_CMD_SDN_EN : PGA112_CMD_SDN_DIS);
    delay_us(10);
}

static void Pga112Device_WriteConfig(Pga112Device *self, uint8_t gain_code, uint8_t channel_code)
{
    if ((self == NULL) || (self->bus == NULL))
    {
        return;
    }

    self->gain_code = gain_code & 0x07U;
    self->channel_code = channel_code & 0x0FU;

    self->bus->ops->write16(self->bus, Pga112Device_BuildWriteWord(self->gain_code, self->channel_code));
    delay_us(10);
}

static void Pga112Device_SetGainCode(Pga112Device *self, uint8_t gain_code)
{
    if (self == NULL)
    {
        return;
    }

    self->ops->write_config(self, gain_code, self->channel_code);
}

static void Pga112Device_SelectChannel(Pga112Device *self, uint8_t channel_code)
{
    if (self == NULL)
    {
        return;
    }

    self->ops->write_config(self, self->gain_code, channel_code);
}

static uint8_t Pga112Device_GetGainCode(Pga112Device *self)
{
    if (self == NULL)
    {
        return PGA112_DEFAULT_GAIN_CODE;
    }

    return self->gain_code;
}

static uint8_t Pga112Device_GetChannelCode(Pga112Device *self)
{
    if (self == NULL)
    {
        return PGA112_ULTRASONIC_CHANNEL;
    }

    return self->channel_code;
}

static uint16_t Pga112Device_GetGainValue(Pga112Device *self, uint8_t gain_code)
{
    (void)self;
    return (uint16_t)(1U << (gain_code & 0x07U));
}

static uint16_t Pga112Device_BuildWriteWord(uint8_t gain_code, uint8_t channel_code)
{
    return (uint16_t)(PGA112_CMD_WRITE |
                      ((uint16_t)(gain_code & 0x07U) << 4) |
                      (uint16_t)(channel_code & 0x0FU));
}

void PGA112_Init(void)
{
    g_pga112_device.ops->init(&g_pga112_device);
}

void PGA112_Shutdown(FunctionalState new_state)
{
    g_pga112_device.ops->shutdown(&g_pga112_device, new_state);
}

void PGA112_WriteConfig(uint8_t gain_code, uint8_t channel_code)
{
    g_pga112_device.ops->write_config(&g_pga112_device, gain_code, channel_code);
}

void PGA112_SetGainCode(uint8_t gain_code)
{
    g_pga112_device.ops->set_gain_code(&g_pga112_device, gain_code);
}

void PGA112_SelectChannel(uint8_t channel_code)
{
    g_pga112_device.ops->select_channel(&g_pga112_device, channel_code);
}

uint8_t PGA112_GetGainCode(void)
{
    return g_pga112_device.ops->get_gain_code(&g_pga112_device);
}

uint8_t PGA112_GetChannelCode(void)
{
    return g_pga112_device.ops->get_channel_code(&g_pga112_device);
}

uint16_t PGA112_GetGainValue(uint8_t gain_code)
{
    return g_pga112_device.ops->get_gain_value(&g_pga112_device, gain_code);
}
