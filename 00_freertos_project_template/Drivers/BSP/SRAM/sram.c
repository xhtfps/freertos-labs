// Drivers/BSP/SRAM/sram.c
// External SRAM/FSMC driver object module.

#include "./BSP/SRAM/sram.h"
#include "./SYSTEM/usart/usart.h"
#include "./XMRAM/XMRAM.h"

SRAM_HandleTypeDef g_sram_handler;

static void SramDevice_Init(SramDevice *self);
static void SramDevice_Write(SramDevice *self, uint8_t *pbuf, uint32_t addr, uint32_t datalen);
static void SramDevice_Read(SramDevice *self, uint8_t *pbuf, uint32_t addr, uint32_t datalen);
static void SramDevice_TestWrite(SramDevice *self, uint32_t addr, uint8_t data);
static uint8_t SramDevice_TestRead(SramDevice *self, uint32_t addr);
static void SramDevice_Construct(SramDevice *self);

static const SramDeviceOps s_sram_device_ops =
{
    SramDevice_Init,
    SramDevice_Write,
    SramDevice_Read,
    SramDevice_TestWrite,
    SramDevice_TestRead
};

SramDevice g_external_sram;

static void SramDevice_Construct(SramDevice *self)
{
    if ((self == NULL) || (self->ops != NULL))
    {
        return;
    }

    self->handle = &g_sram_handler;
    self->base_addr = SRAM_BASE_ADDR;
    self->fsmc_nex = SRAM_FSMC_NEX;
    self->ops = &s_sram_device_ops;
}

// Initialize external SRAM and FSMC timing.
static void SramDevice_Init(SramDevice *self)
{
    GPIO_InitTypeDef gpio_init_struct;
    FSMC_NORSRAM_TimingTypeDef fsmc_readwritetim;

    if ((self == NULL) || (self->handle == NULL))
    {
        return;
    }

    XmRamInit();
    SRAM_CS_GPIO_CLK_ENABLE();
    SRAM_WR_GPIO_CLK_ENABLE();
    SRAM_RD_GPIO_CLK_ENABLE();
    __HAL_RCC_FSMC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio_init_struct.Pin = SRAM_CS_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(SRAM_CS_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = SRAM_WR_GPIO_PIN;
    HAL_GPIO_Init(SRAM_WR_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = SRAM_RD_GPIO_PIN;
    HAL_GPIO_Init(SRAM_RD_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
                           GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
                           GPIO_PIN_14 | GPIO_PIN_15;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOD, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
                           GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
                           GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
                           GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOG, &gpio_init_struct);

    self->handle->Instance = FSMC_NORSRAM_DEVICE;
    self->handle->Extended = FSMC_NORSRAM_EXTENDED_DEVICE;

    if (self->fsmc_nex == 1U)
    {
        self->handle->Init.NSBank = FSMC_NORSRAM_BANK1;
    }
    else if (self->fsmc_nex == 2U)
    {
        self->handle->Init.NSBank = FSMC_NORSRAM_BANK2;
    }
    else if (self->fsmc_nex == 3U)
    {
        self->handle->Init.NSBank = FSMC_NORSRAM_BANK3;
    }
    else
    {
        self->handle->Init.NSBank = FSMC_NORSRAM_BANK4;
    }

    self->handle->Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    self->handle->Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    self->handle->Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
    self->handle->Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    self->handle->Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    self->handle->Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    self->handle->Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    self->handle->Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    self->handle->Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
    self->handle->Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    self->handle->Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;

    fsmc_readwritetim.AddressSetupTime = 0x00;
    fsmc_readwritetim.AddressHoldTime = 0x00;
    fsmc_readwritetim.DataSetupTime = 0x06;
    fsmc_readwritetim.BusTurnAroundDuration = 0x00;
    fsmc_readwritetim.AccessMode = FSMC_ACCESS_MODE_A;
    (void)HAL_SRAM_Init(self->handle, &fsmc_readwritetim, &fsmc_readwritetim);
}

// Write a byte buffer to external SRAM at the specified offset.
static void SramDevice_Write(SramDevice *self, uint8_t *pbuf, uint32_t addr, uint32_t datalen)
{
    if ((self == NULL) || (pbuf == NULL))
    {
        return;
    }

    for (; datalen != 0U; datalen--)
    {
        *(volatile uint8_t *)(self->base_addr + addr) = *pbuf;
        addr++;
        pbuf++;
    }
}

// Read a byte buffer from external SRAM at the specified offset.
static void SramDevice_Read(SramDevice *self, uint8_t *pbuf, uint32_t addr, uint32_t datalen)
{
    if ((self == NULL) || (pbuf == NULL))
    {
        return;
    }

    for (; datalen != 0U; datalen--)
    {
        *pbuf++ = *(volatile uint8_t *)(self->base_addr + addr);
        addr++;
    }
}

// Write one byte to external SRAM at the specified offset.
static void SramDevice_TestWrite(SramDevice *self, uint32_t addr, uint8_t data)
{
    if (self == NULL)
    {
        return;
    }

    self->ops->write(self, &data, addr, 1U);
}

// Read one byte from external SRAM at the specified offset.
static uint8_t SramDevice_TestRead(SramDevice *self, uint32_t addr)
{
    uint8_t data = 0U;

    if (self == NULL)
    {
        return data;
    }

    self->ops->read(self, &data, addr, 1U);
    return data;
}

void sram_init(void)
{
    SramDevice_Construct(&g_external_sram);
    g_external_sram.ops->init(&g_external_sram);
}

void sram_write(uint8_t *pbuf, uint32_t addr, uint32_t datalen)
{
    SramDevice_Construct(&g_external_sram);
    g_external_sram.ops->write(&g_external_sram, pbuf, addr, datalen);
}

void sram_read(uint8_t *pbuf, uint32_t addr, uint32_t datalen)
{
    SramDevice_Construct(&g_external_sram);
    g_external_sram.ops->read(&g_external_sram, pbuf, addr, datalen);
}

void sram_test_write(uint32_t addr, uint8_t data)
{
    SramDevice_Construct(&g_external_sram);
    g_external_sram.ops->test_write(&g_external_sram, addr, data);
}

uint8_t sram_test_read(uint32_t addr)
{
    SramDevice_Construct(&g_external_sram);
    return g_external_sram.ops->test_read(&g_external_sram, addr);
}
