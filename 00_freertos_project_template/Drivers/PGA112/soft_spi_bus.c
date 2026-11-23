/**
 * @file Drivers/PGA112/soft_spi_bus.c
 * @brief PGA112 专用软件 SPI 总线对象模块。
 * @details
 * - 属于 PGA112 增益控制链路，分离设备语义和软件 SPI 总线时序。
 * - 测量服务只关心增益码和通道码，底层帧格式与 GPIO 翻转保留在驱动内部。
 * - 旧 API 保留为兼容层，新代码优先使用对象接口。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./PGA112/soft_spi_bus.h"
#include "./PGA112/SPI.h"
#include "./SYSTEM/delay/delay.h"

static void SoftSpiBus_Init(SoftSpiBus *self, uint8_t mode);
static void SoftSpiBus_SetDioOutput(SoftSpiBus *self);
static void SoftSpiBus_SetDioInput(SoftSpiBus *self);
static void SoftSpiBus_StartFrame(SoftSpiBus *self);
static void SoftSpiBus_EndFrame(SoftSpiBus *self);
static void SoftSpiBus_SendData(SoftSpiBus *self, uint32_t tx_data, uint8_t length, uint8_t pha);
static uint16_t SoftSpiBus_Transfer16(SoftSpiBus *self, uint16_t tx_data);
static void SoftSpiBus_Write16(SoftSpiBus *self, uint16_t tx_data);
static void SoftSpiBus_SetIdle(SoftSpiBus *self);
static void SoftSpiBus_ShiftBits(SoftSpiBus *self, uint32_t tx_data, uint8_t length, uint8_t pha);

static const SoftSpiBusOps s_pga112_spi_bus_ops =
{
    SoftSpiBus_Init,
    SoftSpiBus_SetDioOutput,
    SoftSpiBus_SetDioInput,
    SoftSpiBus_StartFrame,
    SoftSpiBus_EndFrame,
    SoftSpiBus_SendData,
    SoftSpiBus_Transfer16,
    SoftSpiBus_Write16
};

SoftSpiBus g_pga112_spi_bus =
{
    USER_SPI_PORT,
    USER_SPI_CS_PIN,
    USER_SPI_SCK_PIN,
    USER_SPI_DIO_PIN,
    &s_pga112_spi_bus_ops
};

static void SoftSpiBus_Init(SoftSpiBus *self, uint8_t mode)
{
    GPIO_InitTypeDef gpio_init;
    (void)mode;

    if (self == NULL)
    {
        return;
    }

    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio_init.Pin = self->cs_pin | self->sck_pin | self->dio_pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(self->port, &gpio_init);

    SoftSpiBus_SetIdle(self);
}

static void SoftSpiBus_SetDioOutput(SoftSpiBus *self)
{
    GPIO_InitTypeDef gpio_init;

    if (self == NULL)
    {
        return;
    }

    gpio_init.Pin = self->dio_pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(self->port, &gpio_init);
}

static void SoftSpiBus_SetDioInput(SoftSpiBus *self)
{
    GPIO_InitTypeDef gpio_init;

    if (self == NULL)
    {
        return;
    }

    gpio_init.Pin = self->dio_pin;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(self->port, &gpio_init);
}

static void SoftSpiBus_StartFrame(SoftSpiBus *self)
{
    if (self == NULL)
    {
        return;
    }

    self->ops->set_dio_output(self);
    HAL_GPIO_WritePin(self->port, self->cs_pin, GPIO_PIN_RESET);
    delay_us(1);
}

static void SoftSpiBus_EndFrame(SoftSpiBus *self)
{
    if (self == NULL)
    {
        return;
    }

    delay_us(1);
    HAL_GPIO_WritePin(self->port, self->cs_pin, GPIO_PIN_SET);
    SoftSpiBus_SetIdle(self);
}

static void SoftSpiBus_SendData(SoftSpiBus *self, uint32_t tx_data, uint8_t length, uint8_t pha)
{
    if (self == NULL)
    {
        return;
    }

    self->ops->start_frame(self);
    SoftSpiBus_ShiftBits(self, tx_data, length, pha);
    self->ops->end_frame(self);
}

static uint16_t SoftSpiBus_Transfer16(SoftSpiBus *self, uint16_t tx_data)
{
    int8_t bit_index;
    uint16_t rx_data = 0U;

    if (self == NULL)
    {
        return 0U;
    }

    for (bit_index = 15; bit_index >= 0; bit_index--)
    {
        HAL_GPIO_WritePin(self->port,
                          self->dio_pin,
                          (((tx_data >> bit_index) & 0x01U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

        delay_us(1);
        HAL_GPIO_WritePin(self->port, self->sck_pin, GPIO_PIN_SET);

        rx_data <<= 1;
        if (HAL_GPIO_ReadPin(self->port, self->dio_pin) != GPIO_PIN_RESET)
        {
            rx_data |= 0x01U;
        }

        delay_us(1);
        HAL_GPIO_WritePin(self->port, self->sck_pin, GPIO_PIN_RESET);
    }

    return rx_data;
}

static void SoftSpiBus_Write16(SoftSpiBus *self, uint16_t tx_data)
{
    if (self == NULL)
    {
        return;
    }

    self->ops->start_frame(self);
    (void)self->ops->transfer16(self, tx_data);
    self->ops->end_frame(self);
}

static void SoftSpiBus_SetIdle(SoftSpiBus *self)
{
    HAL_GPIO_WritePin(self->port, self->cs_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(self->port, self->sck_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(self->port, self->dio_pin, GPIO_PIN_RESET);
    self->ops->set_dio_output(self);
}

static void SoftSpiBus_ShiftBits(SoftSpiBus *self, uint32_t tx_data, uint8_t length, uint8_t pha)
{
    int16_t bit_index;

    if ((length == 0U) || (length > 32U))
    {
        return;
    }

    for (bit_index = (int16_t)length - 1; bit_index >= 0; bit_index--)
    {
        HAL_GPIO_WritePin(self->port, self->sck_pin, (pha == 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(self->port,
                          self->dio_pin,
                          (((tx_data >> bit_index) & 0x01U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        delay_us(1);
        HAL_GPIO_WritePin(self->port, self->sck_pin, (pha == 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        delay_us(1);
    }
}
