/**
 * @file Drivers/BSP/AD9959/AD9959.c
 * @brief AD9959 四通道 DDS 信号源驱动对象模块。
 * @details
 * - 封装 DDS 的 GPIO 写时序、寄存器写入和频率/幅度/相位字转换。
 * - 使用 Ad9959DeviceOps 作为 C 风格方法表，旧命名函数只做兼容转发。
 * - 参考时钟、通道映射或引脚变化应收敛在本驱动边界内。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/AD9959/AD9959.h"
#include "./SYSTEM/delay/delay.h"

/* ========================================================================== */
/* 宏定义与核心参数配置                                                         */
/* ========================================================================== */

#define AD9959_MAX_SYSCLK_HZ          500000000UL   /**< AD9959 最大允许系统时钟: 500MHz */
#define AD9959_FTW_SCALE              8.589934592f  /**< 频率控制字(FTW)换算系数：2^32 / 500,000,000 */
#define AD9959_MAX_AMPLITUDE_MV       576U          /**< 芯片支持的最大输出幅值 (单位: mV) */
#define AD9959_MAX_AMPLITUDE_CODE     1023U         /**< 10位幅值控制字(ACR)最大值: 0x3FF */
#define AD9959_AMPLITUDE_ENABLE_BIT   0x1000UL      /**< ACR 寄存器中的幅值乘法器使能位 (Bit 12) */
#define AD9959_PHASE_SCALE            45.511111f    /**< 相位控制字(POW)换算系数：14位精度 2^14 / 360 */

/**
 * @brief 硬件引脚快速写宏定义
 */
#define AD9959_WRITE_PIN(port, pin, value) \
    HAL_GPIO_WritePin((port), (pin), ((value) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET)

/* ========================================================================== */
/* 内部静态私有函数声明                                                         */
/* ========================================================================== */

static uint8_t AD9959_GetChannelMask(uint8_t channel);
static void AD9959_WritePin(GPIO_TypeDef *port, uint16_t pin, uint8_t value);
static void AD9959_WriteBit(uint8_t value);
static void AD9959_FillFrequencyWord(uint32_t freq, uint8_t data[4]);
static void AD9959_FillAmplitudeWord(uint16_t amplitude, uint8_t data[3]);
static void AD9959_FillPhaseWord(uint16_t phase, uint8_t data[2]);
static void AD9959_WriteChannelRegister(uint8_t channel, uint8_t register_address,
                                        uint8_t register_count, uint8_t *register_data,
                                        uint8_t update);

/* ========================================================================== */
/* 芯片寄存器默认/缓存数据表                                                    */
/* ========================================================================== */

/* CSR (通道选择寄存器) 数据映射：用于决定当前操作哪个或哪些通道 */
uint8_t CSR_DATA0[1]   = {0x10U};   /**< 仅选择通道 0 (CH0) */
uint8_t CSR_DATA1[1]   = {0x20U};   /**< 仅选择通道 1 (CH1) */
uint8_t CSR_DATA2[1]   = {0x40U};   /**< 仅选择通道 2 (CH2) */
uint8_t CSR_DATA3[1]   = {0x80U};   /**< 仅选择通道 3 (CH3) */
uint8_t CSR_DATAall[1] = {0xF0U};   /**< 全选通道 (CH0 ~ CH3 并行写入) */

uint8_t FR2_DATA[2]    = {0x00U, 0x00U};            /**< 功能寄存器 2 缓存值 */
uint8_t CFR_DATA[3]    = {0x00U, 0x03U, 0x02U};     /**< 通道功能寄存器缓存值 (配置调制模式等) */

/* POW (相位偏移寄存器) 四通道缓存 */
uint8_t CPOW0_DATA0[2] = {0x00U, 0x00U};            /**< CH0 相位控制字缓存 */
uint8_t CPOW0_DATA1[2] = {0x00U, 0x00U};            /**< CH1 相位控制字缓存 */
uint8_t CPOW0_DATA2[2] = {0x00U, 0x00U};            /**< CH2 相位控制字缓存 */
uint8_t CPOW0_DATA3[2] = {0x00U, 0x00U};            /**< CH3 相位控制字缓存 */

/* 扫频/线性调制寄存器缓存 */
uint8_t LSRR_DATA[2]   = {0x00U, 0x00U};            /**< 线性扫频速率寄存器 */
uint8_t RDW_DATA[4]    = {0x00U, 0x00U, 0x00U, 0x00U}; /**< 线性扫频步进上升寄存器 */
uint8_t FDW_DATA[4]    = {0x00U, 0x00U, 0x00U, 0x00U}; /**< 线性扫频步进下降寄存器 */

/* 当前计算结果的全局/临时工作缓存 */
uint8_t CFTW0_DATA[4]  = {0x00U, 0x00U, 0x00U, 0x00U}; /**< 频率控制字临时缓存 (4字节) */
uint8_t ACR_DATA[3]    = {0x00U, 0x00U, 0x00U};     /**< 幅度控制寄存器临时缓存 (3字节) */

/* ========================================================================== */
/* 硬件控制及初始化函数实现                                                     */
/* ========================================================================== */

/**
 * @brief  初始化 AD9959 相关的 STM32 GPIO 引脚
 * @note   将所有的控制线（如 CS、SCLK、SDIO、UPDATE、RESET等）配置为推挽输出模式
 */
void AD9959_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    /* 开启 GPIOB, GPIOE, GPIOF 的外设时钟 */
    AD9959_GPIOB_CLK_ENABLE();
    AD9959_GPIOE_CLK_ENABLE();
    AD9959_GPIOF_CLK_ENABLE();

    gpio_init_struct.Mode  = GPIO_MODE_OUTPUT_PP;   /**< 推挽输出 */
    gpio_init_struct.Pull  = GPIO_NOPULL;          /**< 不带上下拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;  /**< 高速模式，确保高频模拟时序稳定 */

    /* 初始化 Port E 上的数据线与控制线 */
    gpio_init_struct.Pin = AD9959_SDIO2_GPIO_PIN | AD9959_SDIO3_GPIO_PIN |
                           AD9959_SDIO1_GPIO_PIN | AD9959_SCLK_GPIO_PIN  |
                           AD9959_SDIO0_GPIO_PIN | AD9959_CS_GPIO_PIN    |
                           AD9959_PS3_GPIO_PIN;
    HAL_GPIO_Init(GPIOE, &gpio_init_struct);

    /* 初始化 Port F 上的状态选择线和复位/电源引脚 */
    gpio_init_struct.Pin = AD9959_PS2_GPIO_PIN   | AD9959_RESET_GPIO_PIN |
                           AD9959_PS1_GPIO_PIN   | AD9959_PWR_GPIO_PIN   |
                           AD9959_PS0_GPIO_PIN;
    HAL_GPIO_Init(GPIOF, &gpio_init_struct);

    /* 初始化 Port B 上的数据异步更新引脚 (UPDATE) */
    gpio_init_struct.Pin = AD9959_UPDATE_GPIO_PIN;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);
}

/**
 * @brief  将 AD9959 的所有控制引脚设置为空闲/默认状态
 * @note   片选 CS 拉高（不选中），时钟 SCLK、更新 UPDATE 以及数据线全部清零
 */
void AD9959_SetIdle(void)
{
    AD9959_WRITE_PIN(AD9959_PWR_GPIO_PORT,    AD9959_PWR_GPIO_PIN,    0U); /**< 关闭掉电模式，正常工作 */
    AD9959_WRITE_PIN(AD9959_CS_GPIO_PORT,     AD9959_CS_GPIO_PIN,     1U); /**< 取消片选 */
    AD9959_WRITE_PIN(AD9959_SCLK_GPIO_PORT,   AD9959_SCLK_GPIO_PIN,   0U); /**< 串行时钟线置低 */
    AD9959_WRITE_PIN(AD9959_UPDATE_GPIO_PORT, AD9959_UPDATE_GPIO_PIN, 0U); /**< 更新线置低 */
    AD9959_WRITE_PIN(AD9959_PS0_GPIO_PORT,    AD9959_PS0_GPIO_PIN,    0U); /**< 调制选择引脚全部清零 */
    AD9959_WRITE_PIN(AD9959_PS1_GPIO_PORT,    AD9959_PS1_GPIO_PIN,    0U);
    AD9959_WRITE_PIN(AD9959_PS2_GPIO_PORT,    AD9959_PS2_GPIO_PIN,    0U);
    AD9959_WRITE_PIN(AD9959_PS3_GPIO_PORT,    AD9959_PS3_GPIO_PIN,    0U);
    AD9959_WRITE_PIN(AD9959_SDIO0_GPIO_PORT,  AD9959_SDIO0_GPIO_PIN,  0U); /**< 数据线置低 */
    AD9959_WRITE_PIN(AD9959_SDIO1_GPIO_PORT,  AD9959_SDIO1_GPIO_PIN,  0U);
    AD9959_WRITE_PIN(AD9959_SDIO2_GPIO_PORT,  AD9959_SDIO2_GPIO_PIN,  0U);
    AD9959_WRITE_PIN(AD9959_SDIO3_GPIO_PORT,  AD9959_SDIO3_GPIO_PIN,  0U);
}

/**
 * @brief  硬件复位 AD9959 芯片
 * @note   产生一个标准的高电平复位脉冲
 */
void AD9959_Reset(void)
{
    AD9959_WRITE_PIN(AD9959_RESET_GPIO_PORT, AD9959_RESET_GPIO_PIN, 0U);
    delay1(1U);
    AD9959_WRITE_PIN(AD9959_RESET_GPIO_PORT, AD9959_RESET_GPIO_PIN, 1U); /**< 拉高进行复位 */
    delay1(30U);
    AD9959_WRITE_PIN(AD9959_RESET_GPIO_PORT, AD9959_RESET_GPIO_PIN, 0U); /**< 重新拉低，释放复位 */
}

/**
 * @brief  向 UPDATE 引脚发送正脉冲
 * @note   AD9959 的内部寄存器分为底层双缓冲，写入新数据后必须执行此操作才能使数据正式生效输出
 */
void AD9959_Update(void)
{
    AD9959_WRITE_PIN(AD9959_UPDATE_GPIO_PORT, AD9959_UPDATE_GPIO_PIN, 0U);
    delay1(2U);
    AD9959_WRITE_PIN(AD9959_UPDATE_GPIO_PORT, AD9959_UPDATE_GPIO_PIN, 1U); /**< 产生上升沿 */
    delay1(4U);
    AD9959_WRITE_PIN(AD9959_UPDATE_GPIO_PORT, AD9959_UPDATE_GPIO_PIN, 0U);
}

/**
 * @brief  通过模拟 SPI 向串行寄存器写入数据
 * @param  register_address: 目标寄存器地址
 * @param  register_count:   写入数据的字节数
 * @param  register_data:    待写入的数据缓冲区指针
 * @param  update:           是否立即执行 IO_Update 更新生效 (1:是, 0:否)
 */
void AD9959_WriteData(uint8_t register_address, uint8_t register_count,
                      const uint8_t *register_data, uint8_t update)
{
    uint8_t register_index;
    uint8_t bit_index;
    uint8_t value_to_write;

    /* 健壮性检查 */
    if ((register_count > 0U) && (register_data == NULL))
    {
        return;
    }

    /* AD9959 串行写入按 MSB first 输出。
     * CS 拉低期间先发 8 位寄存器地址，再连续发 register_count 个数据字节。
     */
    AD9959_WRITE_PIN(AD9959_SCLK_GPIO_PORT, AD9959_SCLK_GPIO_PIN, 0U);
    AD9959_WRITE_PIN(AD9959_CS_GPIO_PORT,   AD9959_CS_GPIO_PIN,   0U); /**< 拉低片选，开始 SPI 传输 */

    /* 第一步：写入 1 字节的指令字（即寄存器地址，高位在前 MSB） */
    value_to_write = register_address;
    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        AD9959_WriteBit((uint8_t)(value_to_write & 0x80U));
        value_to_write <<= 1;
    }

    /* 第二步：循环串行写入多字节的数据内容 */
    for (register_index = 0U; register_index < register_count; register_index++)
    {
        value_to_write = register_data[register_index];
        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            AD9959_WriteBit((uint8_t)(value_to_write & 0x80U));
            value_to_write <<= 1;
        }
    }

    AD9959_WRITE_PIN(AD9959_SCLK_GPIO_PORT, AD9959_SCLK_GPIO_PIN, 0U);
    
    if (update == 1U)
    {
        AD9959_Update(); /**< 如果要求立即生效，则触发更新脉冲 */
    }
    AD9959_WRITE_PIN(AD9959_CS_GPIO_PORT, AD9959_CS_GPIO_PIN, 1U); /**< 释放片选 */
}

/**
 * @brief  AD9959 芯片系统层面的核心初始化流程
 * @note   包含引脚配置、时钟锁相环(PLL)倍频配置、多通道初始化及默认波形参数下发
 */
void AD9959_Init(void)
{
    uint8_t cftw0_data[4] = {0x00U, 0x00U, 0x00U, 0x00U};
    uint8_t acr_data[3]   = {0x00U, 0x00U, 0x00U};
    
    /* FR1 (功能寄存器1) 配置数据：0xD00000 
     * 含义：开启充电泵电流，使能 20 倍频 PLL（外接25MHz晶振时：25M * 20 = 500MHz系统主频）
     */
    uint8_t fr1_data[3]   = {0xD0U, 0x00U, 0x00U};

    AD9959_GPIO_Init();
    AD9959_SetIdle();
    delay_ms(50U);
    AD9959_Reset();
    delay_ms(50U);

    /* 1. 下发倍频控制字，锁定 500MHz 全局工作时钟 */
    AD9959_WriteData(AD9959_FR1_ADD, 3U, fr1_data, 1U);

    /* 2. 预设四通道的初始频率为 50kHz */
    AD9959_SetFrequency(AD9959_CHANNEL_0, 50000UL);
    AD9959_SetFrequency(AD9959_CHANNEL_1, 50000UL);
    AD9959_SetFrequency(AD9959_CHANNEL_2, 50000UL);
    AD9959_SetFrequency(AD9959_CHANNEL_3, 50000UL);

    /* 3. 产生特定的试验数据字 (频率 1MHz, 幅度 300mV) */
    AD9959_FillFrequencyWord(1000000UL, cftw0_data);
    AD9959_FillAmplitudeWord(300U, acr_data);

    /* 分别对通道 0 和 通道 2 进行显式的波形特征下发（测试硬件链路） */
    AD9959_WriteChannelRegister(AD9959_CHANNEL_0, AD9959_CFTW0_ADD, 4U, cftw0_data, 1U);
    AD9959_WriteData(AD9959_ACR_ADD, 3U, acr_data, 1U);
    AD9959_WriteData(AD9959_CPOW0_ADD, 2U, CPOW0_DATA0, 1U);

    AD9959_WriteChannelRegister(AD9959_CHANNEL_2, AD9959_CFTW0_ADD, 4U, cftw0_data, 1U);
    AD9959_WriteData(AD9959_ACR_ADD, 3U, acr_data, 1U);
    AD9959_WriteData(AD9959_CPOW0_ADD, 2U, CPOW0_DATA2, 1U);

    /* 4. 分步设置 0、1、2、3 通道呈现 30° 正交相位差级联 */
    AD9959_SetPhase(AD9959_CHANNEL_0, 0U);
    AD9959_SetPhase(AD9959_CHANNEL_1, 30U);
    AD9959_SetPhase(AD9959_CHANNEL_2, 60U);
    AD9959_SetPhase(AD9959_CHANNEL_3, 90U);

    /* 5. 分别配置四通道的输出幅值 */
    AD9959_SetAmplitude(AD9959_CHANNEL_0, 100U);
    AD9959_SetAmplitude(AD9959_CHANNEL_1, 150U);
    AD9959_SetAmplitude(AD9959_CHANNEL_2, 200U);
    AD9959_SetAmplitude(AD9959_CHANNEL_3, 576U);

    /* 6. 最终覆盖配置四通道的工作频率 */
    AD9959_SetFrequency(AD9959_CHANNEL_0, 50000UL);
    AD9959_SetFrequency(AD9959_CHANNEL_1, 60000UL);
    AD9959_SetFrequency(AD9959_CHANNEL_2, 70000UL);
    AD9959_SetFrequency(AD9959_CHANNEL_3, 1UL);
}

/* ========================================================================== */
/* 高级 API 封装层（提供给外部调用修改 波形三要素）                              */
/* ========================================================================== */

/** @brief 设置通道输出频率并立即更新生效 */
void AD9959_SetFrequency(uint8_t channel, uint32_t freq)
{
    AD9959_SetFrequencyNoUpdate(channel, freq);
    AD9959_Update();
}

/** @brief 设置通道输出频率（暂不更新生效，用于多通道同步联调） */
void AD9959_SetFrequencyNoUpdate(uint8_t channel, uint32_t freq)
{
    AD9959_FillFrequencyWord(freq, CFTW0_DATA);
    AD9959_WriteChannelRegister(channel, AD9959_CFTW0_ADD, 4U, CFTW0_DATA, 0U);
}

/** @brief 设置通道输出幅值并立即更新生效 */
void AD9959_SetAmplitude(uint8_t channel, uint16_t amplitude)
{
    AD9959_SetAmplitudeNoUpdate(channel, amplitude);
    AD9959_Update();
}

/** @brief 设置通道输出幅值（暂不更新生效） */
void AD9959_SetAmplitudeNoUpdate(uint8_t channel, uint16_t amplitude)
{
    AD9959_FillAmplitudeWord(amplitude, ACR_DATA);
    AD9959_WriteChannelRegister(channel, AD9959_ACR_ADD, 3U, ACR_DATA, 0U);
}

/** @brief 设置通道输出相位并立即更新生效 */
void AD9959_SetPhase(uint8_t channel, uint16_t phase)
{
    AD9959_SetPhaseNoUpdate(channel, phase);
    AD9959_Update();
}

/** @brief 设置通道输出相位（暂不更新生效） */
void AD9959_SetPhaseNoUpdate(uint8_t channel, uint16_t phase)
{
    AD9959_FillPhaseWord(phase, CPOW0_DATA0);
    AD9959_WriteChannelRegister(channel, AD9959_CPOW0_ADD, 2U, CPOW0_DATA0, 0U);
}

/**
 * @brief 简单的粗略微秒级阻塞延时函数
 */
void delay1(uint32_t length)
{
    volatile uint32_t count = length * 12U;
    while (count-- != 0U)
    {
    }
}

/* ========================================================================== */
/* 旧版/兼容性代码外壳 (与上述 API 功能完全等价，常用于历史版本接口兼容)            */
/* ========================================================================== */

void IntReset(void)                                 { AD9959_Reset(); }
void IO_Update(void)                                { AD9959_Update(); }
void Intserve(void)                                 { AD9959_SetIdle(); }
void Init_AD9959(void)                              { AD9959_Init(); }
void Write_frequence(uint8_t Channel, uint32_t Freq){ AD9959_SetFrequency(Channel, Freq); }
void Write_frequence_no_update(uint8_t Channel, uint32_t Freq) { AD9959_SetFrequencyNoUpdate(Channel, Freq); }
void Write_Amplitude(uint8_t Channel, uint16_t Ampli)          { AD9959_SetAmplitude(Channel, Ampli); }
void Write_Amplitude_no_update(uint8_t Channel, uint16_t Ampli) { AD9959_SetAmplitudeNoUpdate(Channel, Ampli); }
void Write_Phase(uint8_t Channel, uint16_t Phase)              { AD9959_SetPhase(Channel, Phase); }
void Write_Phase_no_update(uint8_t Channel, uint16_t Phase)     { AD9959_SetPhaseNoUpdate(Channel, Phase); }

void WriteData_AD9959(uint8_t RegisterAddress, uint8_t NumberofRegisters, uint8_t *RegisterData, uint8_t temp)
{
    AD9959_WriteData(RegisterAddress, NumberofRegisters, RegisterData, temp);
}

/* ========================================================================== */
/* 内部私有工具函数底层实现                                                     */
/* ========================================================================== */

/**
 * @brief  根据传入的自定义通道代码获取芯片 CSR 要求的通道掩码
 * @return 对应寄存器所需的实际 8 位掩码数据
 */
static uint8_t AD9959_GetChannelMask(uint8_t channel)
{
    switch (channel)
    {
        case AD9959_CHANNEL_0:   return CSR_DATA0[0];
        case AD9959_CHANNEL_1:   return CSR_DATA1[0];
        case AD9959_CHANNEL_2:   return CSR_DATA2[0];
        case AD9959_CHANNEL_3:   return CSR_DATA3[0];
        case AD9959_CHANNEL_ALL: return CSR_DATAall[0];
        default:                 return 0U;
    }
}

/** @brief 封装底层的 GPIO 写入操作 */
static void AD9959_WritePin(GPIO_TypeDef *port, uint16_t pin, uint8_t value)
{
    AD9959_WRITE_PIN(port, pin, value);
}

/**
 * @brief  模拟串行总线发送单个 Bit (在 SCLK 的上升沿被芯片采样)
 */
static void AD9959_WriteBit(uint8_t value)
{
    AD9959_WritePin(AD9959_SCLK_GPIO_PORT,  AD9959_SCLK_GPIO_PIN,  0U); /**< 时钟线拉低 */
    AD9959_WritePin(AD9959_SDIO0_GPIO_PORT, AD9959_SDIO0_GPIO_PIN, value); /**< 准备数据位 */
    AD9959_WritePin(AD9959_SCLK_GPIO_PORT,  AD9959_SCLK_GPIO_PIN,  1U); /**< 时钟线拉高，芯片在其上升沿捕获数据 */
}

/**
 * @brief  将频率值(Hz)转化为 32 位 AD9959 频率控制字(FTW)，拆解到 4 字节数组中
 * @note   数学公式：FTW = (Desired Output Frequency * 2^32) / SYSCLK
 */
static void AD9959_FillFrequencyWord(uint32_t freq, uint8_t data[4])
{
    uint32_t temp;

    if (freq > AD9959_MAX_SYSCLK_HZ)
    {
        freq = AD9959_MAX_SYSCLK_HZ; /* 超过系统时钟则限幅处理 */
    }

    temp = (uint32_t)((float)freq * AD9959_FTW_SCALE);
    
    /* 大端序(MSB First)：高字节存放在数组低索引处 */
    data[3] = (uint8_t)temp;
    data[2] = (uint8_t)(temp >> 8);
    data[1] = (uint8_t)(temp >> 16);
    data[0] = (uint8_t)(temp >> 24);
}

/**
 * @brief  将目标幅值(mV)转化为 10 位幅值控制字并填充到 3 字节的 ACR 寄存器包中
 */
static void AD9959_FillAmplitudeWord(uint16_t amplitude, uint8_t data[3])
{
    uint32_t temp;

    if (amplitude > AD9959_MAX_AMPLITUDE_MV)
    {
        amplitude = AD9959_MAX_AMPLITUDE_MV;
    }

    /* 线性比例换算 10位码：Code = (Ampli_mv * 1023) / 576 */
    temp = ((uint32_t)amplitude * AD9959_MAX_AMPLITUDE_CODE) / AD9959_MAX_AMPLITUDE_MV;
    temp |= AD9959_AMPLITUDE_ENABLE_BIT; /**< 强行附带使能乘法器位，保证幅值设置生效 */

    /* 装填进 24 位的 ACR 格式数组（大端序） */
    data[0] = 0x00U;
    data[1] = (uint8_t)((temp >> 8) | 0x10U); /**< 0x10 表示开启手动幅值控制模式 */
    data[2] = (uint8_t)temp;
}

/**
 * @brief  将角度制相位(0~360°)转化为 14 位相位控制字(POW)并分配到 2 字节数组中
 * @note   数学公式：POW = (Desired Phase Angle * 2^14) / 360
 */
static void AD9959_FillPhaseWord(uint16_t phase, uint8_t data[2])
{
    uint16_t temp;

    if (phase > 360U)
    {
        phase = 360U;
    }

    temp = (uint16_t)((float)phase * AD9959_PHASE_SCALE);
    
    /* 大端序填充 */
    data[1] = (uint8_t)temp;
    data[0] = (uint8_t)(temp >> 8);
}

/**
 * @brief  定向向特定的单通道或多通道独立寄存器写入配置数据
 * @note   其核心原理是先往全局 CSR 寄存器写入通道掩码激活对应通道，再下发具体业务寄存器数据
 */
static void AD9959_WriteChannelRegister(uint8_t channel, uint8_t register_address,
                                        uint8_t register_count, uint8_t *register_data,
                                        uint8_t update)
{
    uint8_t channel_mask = AD9959_GetChannelMask(channel);

    if (channel_mask == 0U)
    {
        return; /* 非法通道指令直接返回 */
    }

    /* 第一步：激活对应的目标通道（暂不更新触发）。
     * CSR 是后续“通道私有寄存器”写入的路由开关，必须先写它再写 CFTW/ACR/CPOW。
     */
    AD9959_WriteData(AD9959_CSR_ADD, 1U, &channel_mask, 0U);
    
    /* 第二步：针对被激活的通道下发实际的功能控制字。
     * update=0 可用于多通道批量写入后统一 IO_UPDATE，实现同步变频/变幅/变相。
     */
    AD9959_WriteData(register_address, register_count, register_data, update);
}
