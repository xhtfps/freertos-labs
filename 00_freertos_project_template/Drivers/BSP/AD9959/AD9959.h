/**
 * @file Drivers/BSP/AD9959/AD9959.h
 * @brief AD9959 四通道 DDS 信号源驱动对象模块。
 * @details
 * - 封装 DDS 的 GPIO 写时序、寄存器写入和频率/幅度/相位字转换。
 * - 使用 Ad9959DeviceOps 作为 C 风格方法表，旧命名函数只做兼容转发。
 * - 参考时钟、通道映射或引脚变化应收敛在本驱动边界内。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __AD9959_H
#define __AD9959_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* 1. AD9959 内部寄存器地址定义 (对应官方 Datasheet 寄存器映射表)                */
/* ========================================================================== */
#define AD9959_CSR_ADD       0x00U  /**< 通道选择寄存器 (Channel Select Register，大小: 1 Byte)
                                         用于选择接下来要把数据写入哪个通道 (CH0~CH3)。不可或缺！ */
#define AD9959_FR1_ADD       0x01U  /**< 功能寄存器 1 (Function Register 1，大小: 3 Bytes)
                                         控制全局 PLL 锁相环倍频系数(如20倍频)、充电泵电流以及 VCO 增益。 */
#define AD9959_FR2_ADD       0x02U  /**< 功能寄存器 2 (Function Register 2，大小: 2 Bytes)
                                         控制多芯片同步、双向扫频模式等高级全局功能。 */
#define AD9959_CFR_ADD       0x03U  /**< 通道功能寄存器 (Channel Function Register，大小: 3 Bytes)
                                         控制当前选定通道的调制模式（2级/4级/16级调制）及数据输入格式（串/并）。 */
#define AD9959_CFTW0_ADD     0x04U  /**< 通道频率控制字 0 (Channel Frequency Tuning Word，大小: 4 Bytes)
                                         最核心的寄存器！输入 32 位控制字来决定该通道最终输出的基波频率 (Hz)。 */
#define AD9959_CPOW0_ADD     0x05U  /**< 通道相位偏移控制字 0 (Channel Phase Offset Word，大小: 2 Bytes)
                                         输入 14 位控制字来调整输出信号的初始相位，常用于多通道正交信号发生。 */
#define AD9959_ACR_ADD       0x06U  /**< 幅度控制寄存器 (Amplitude Control Register，大小: 3 Bytes)
                                         控制输出信号的幅值大小 (mV)，包含 10 位乘法器 DAC 控制及手动/自动数字斜率。 */
#define AD9959_LSRR_ADD      0x07U  /**< 线性扫频速率寄存器 (Linear Sweep Ramp Rate，大小: 2 Bytes)
                                         在扫频模式下，用来定义频率/相位/幅度步进变化的采样时钟计数器间隔。 */
#define AD9959_RDW_ADD       0x08U  /**< 上升累加步进寄存器 (Rising Delta Word，大小: 4 Bytes)
                                         定义扫频信号在上升阶段（递增）时，每一次步进累加的频率、相位或幅度增量。 */
#define AD9959_FDW_ADD       0x09U  /**< 下降累加步进寄存器 (Falling Delta Word，大小: 4 Bytes)
                                         定义扫频信号在下降阶段（递减）时，每一次步进扣除的频率、相位或幅度减量。 */

/* ========================================================================== */
/* 2. 历史版本寄存器别名兼容 (完全等价映射，专门用于照顾老代码或直接移植的代码)     */
/* ========================================================================== */
#define CSR_ADD              AD9959_CSR_ADD
#define FR1_ADD              AD9959_FR1_ADD
#define FR2_ADD              AD9959_FR2_ADD
#define CFR_ADD              AD9959_CFR_ADD
#define CFTW0_ADD            AD9959_CFTW0_ADD
#define CPOW0_ADD            AD9959_CPOW0_ADD
#define ACR_ADD              AD9959_ACR_ADD
#define LSRR_ADD             AD9959_LSRR_ADD
#define RDW_ADD              AD9959_RDW_ADD
#define FDW_ADD              AD9959_FDW_ADD

/* ========================================================================== */
/* 3. STM32 GPIO 硬件引脚物理映射配置 (请务必与板子实际PCB走线一一对应)           */
/* ========================================================================== */
/* ---- 模拟 SPI 串行控制主线 ---- */
#define AD9959_CS_GPIO_PORT           GPIOE
#define AD9959_CS_GPIO_PIN            GPIO_PIN_5   /**< PE5 -> CS 片选信号 (低电平有效，用于使能/禁止总线通信) */
#define AD9959_SCLK_GPIO_PORT         GPIOE
#define AD9959_SCLK_GPIO_PIN          GPIO_PIN_3   /**< PE3 -> SCLK 串行时钟线 (上升沿时数据被锁存入硬件) */
#define AD9959_UPDATE_GPIO_PORT       GPIOB
#define AD9959_UPDATE_GPIO_PIN        GPIO_PIN_0   /**< PB0 -> UPDATE 异步数据更新触发引脚 (上升沿将寄存器缓存同步到输出) */

/* ---- 调制配置引脚 (Profile Select Pins，用于多通道键控调制，如 FSK/ASK/PSK 硬件快速切换) ---- */
#define AD9959_PS0_GPIO_PORT          GPIOF
#define AD9959_PS0_GPIO_PIN           GPIO_PIN_5   /**< PF5 -> PS0 控制端，对应 Channel 0 */
#define AD9959_PS1_GPIO_PORT          GPIOF
#define AD9959_PS1_GPIO_PIN           GPIO_PIN_3   /**< PF3 -> PS1 控制端，对应 Channel 1 */
#define AD9959_PS2_GPIO_PORT          GPIOF
#define AD9959_PS2_GPIO_PIN           GPIO_PIN_1   /**< PF1 -> PS2 控制端，对应 Channel 2 */
#define AD9959_PS3_GPIO_PORT          GPIOE
#define AD9959_PS3_GPIO_PIN           GPIO_PIN_6   /**< PE6 -> PS3 控制端，对应 Channel 3 */

/* ---- 模拟 SPI 数据线 (AD9959支持 1线/2线/4线 串行模式，当前初始化通常使用 SDIO0 发送数据) ---- */
#define AD9959_SDIO0_GPIO_PORT        GPIOE
#define AD9959_SDIO0_GPIO_PIN         GPIO_PIN_4   /**< PE4 -> SDIO0 串行数据输入/输出线 0 (核心单线串行写入线) */
#define AD9959_SDIO1_GPIO_PORT        GPIOE
#define AD9959_SDIO1_GPIO_PIN         GPIO_PIN_2   /**< PE2 -> SDIO1 串行数据输入/输出线 1 */
#define AD9959_SDIO2_GPIO_PORT        GPIOE
#define AD9959_SDIO2_GPIO_PIN         GPIO_PIN_0   /**< PE0 -> SDIO2 串行数据输入/输出线 2 */
#define AD9959_SDIO3_GPIO_PORT        GPIOE
#define AD9959_SDIO3_GPIO_PIN         GPIO_PIN_1   /**< PE1 -> SDIO3 串行数据输入/输出线 3 */

/* ---- 芯片电源管理与物理复位控制 ---- */
#define AD9959_PWR_GPIO_PORT          GPIOF
#define AD9959_PWR_GPIO_PIN           GPIO_PIN_4   /**< PF4 -> PWR/PDC 掉电控制引脚 (高电平使芯片休眠，当前程序常置低) */
#define AD9959_RESET_GPIO_PORT        GPIOF
#define AD9959_RESET_GPIO_PIN         GPIO_PIN_2   /**< PF2 -> RESET 硬件复位引脚 (高电平有效，复位后寄存器恢复默认) */

/* ========================================================================== */
/* 4. STM32 寄存器层面的 GPIO 外设时钟使能宏                                    */
/* ========================================================================== */
#define AD9959_GPIOB_CLK_ENABLE()     do { __HAL_RCC_GPIOB_CLK_ENABLE(); } while (0) /**< 开启 B 组端口时钟 (用于 UPDATE) */
#define AD9959_GPIOE_CLK_ENABLE()     do { __HAL_RCC_GPIOE_CLK_ENABLE(); } while (0) /**< 开启 E 组端口时钟 (用于 CS/SCLK/SDIO) */
#define AD9959_GPIOF_CLK_ENABLE()     do { __HAL_RCC_GPIOF_CLK_ENABLE(); } while (0) /**< 开启 F 组端口时钟 (用于 RESET/PWR/PS) */

/* ========================================================================== */
/* 5. AD9959 逻辑通道号枚举常数定义                                              */
/* ========================================================================== */
#define AD9959_CHANNEL_0              0U  /**< 代表逻辑通道 0 (CH0) */
#define AD9959_CHANNEL_1              1U  /**< 代表逻辑通道 1 (CH1) */
#define AD9959_CHANNEL_2              2U  /**< 代表逻辑通道 2 (CH2) */
#define AD9959_CHANNEL_3              3U  /**< 代表逻辑通道 3 (CH3) */
#define AD9959_CHANNEL_ALL            4U  /**< 代表全选四通道 (进行同步广播群写配置，效率更高) */

/* ========================================================================== */
/* 6. 外部全局变量声明 (映射并在 .c 文件中实例化)                                 */
/* ========================================================================== */
extern uint8_t CSR_DATAall[1];  /**< 四通道全选掩码缓存数组，内容为 {0xF0U} */
extern uint8_t CFTW0_DATA[4];   /**< 存放 32 位频率控制字的 4 字节临时计算缓冲区 */
extern uint8_t ACR_DATA[3];     /**< 存放 24 位幅度控制字的 3 字节临时计算缓冲区 */

typedef struct Ad9959Device Ad9959Device;

/**
 * @brief AD9959 设备对象操作表。
 * @details
 * 写频率/幅度/相位时分为“立即 update”和“暂不 update”两组接口，
 * 便于多通道参数先写入缓存，再统一 IO_UPDATE 达到同步生效。
 */
typedef struct
{
    void (*gpio_init)(Ad9959Device *self); /**< 初始化 AD9959 相关 GPIO。 */
    void (*set_idle)(Ad9959Device *self);  /**< 将片选、时钟、数据、复位等控制线拉回空闲状态。 */
    void (*reset)(Ad9959Device *self);     /**< 产生硬件复位脉冲。 */
    void (*update)(Ad9959Device *self);    /**< 产生 IO_UPDATE 脉冲，使已写寄存器内容生效。 */
    void (*write_data)(Ad9959Device *self, uint8_t register_address, uint8_t register_count,
                       const uint8_t *register_data, uint8_t update);                  /**< 写指定寄存器，可选立即同步。 */
    void (*init)(Ad9959Device *self);                                                   /**< 完整初始化芯片和默认输出。 */
    void (*set_frequency)(Ad9959Device *self, uint8_t channel, uint32_t freq);           /**< 设置频率并立即生效。 */
    void (*set_frequency_no_update)(Ad9959Device *self, uint8_t channel, uint32_t freq); /**< 设置频率但暂不 IO_UPDATE。 */
    void (*set_amplitude)(Ad9959Device *self, uint8_t channel, uint16_t amplitude);      /**< 设置幅度并立即生效。 */
    void (*set_amplitude_no_update)(Ad9959Device *self, uint8_t channel, uint16_t amplitude); /**< 设置幅度但暂不 IO_UPDATE。 */
    void (*set_phase)(Ad9959Device *self, uint8_t channel, uint16_t phase);              /**< 设置相位并立即生效。 */
    void (*set_phase_no_update)(Ad9959Device *self, uint8_t channel, uint16_t phase);    /**< 设置相位但暂不 IO_UPDATE。 */
} Ad9959DeviceOps;

/**
 * @brief AD9959 设备对象。
 * @note 当前对象不保存运行状态，保留结构体是为了统一后续扩展和 ops/self 调用。
 */
struct Ad9959Device
{
    const Ad9959DeviceOps *ops; /**< DDS 设备方法表。 */
};

extern Ad9959Device g_ad9959_device;

/* ========================================================================== */
/* 7. 标准驱动核心 API 函数声明 (推荐新业务代码优先调用这一套)                      */
/* ========================================================================== */

/**
 * @brief  初始化 AD9959 芯片所占用的所有 STM32 GPIO 引脚配置。
 * @note   内部会将其配置为高速推挽输出、无上下拉模式。
 */
void AD9959_GPIO_Init(void);

/**
 * @brief  将 AD9959 的所有控制引脚拉回至初始默认状态（取消片选、时钟/数据清零）。
 */
void AD9959_SetIdle(void);

/**
 * @brief  触发一次 AD9959 硬件全局复位脉冲。
 */
void AD9959_Reset(void);

/**
 * @brief  给 UPDATE 引脚发送一个高电平脉冲，将寄存器缓冲池里的波形参数正式推送到硬件模拟输出端。
 */
void AD9959_Update(void);

/**
 * @brief  底层模拟 SPI 核心数据传输函数。
 * @param  register_address: 目标寄存器地址 (如 AD9959_CFTW0_ADD)
 * @param  register_count:   本次操作写入的数据字节长度
 * @param  register_data:    待写入的数据源数组首地址
 * @param  update:           决定是否在写入数据后立刻触发更新脉冲 (1:立刻生效, 0:暂不生效)
 */
void AD9959_WriteData(uint8_t register_address, uint8_t register_count,
                      const uint8_t *register_data, uint8_t update);

/**
 * @brief  整个 DDS 系统的核心初始化流程。
 * @note   包含引脚配置、开启主频锁相环、设置四通道 50kHz 初始默认波形。
 */
void AD9959_Init(void);

/* ---- 频率设置核心接口 ---- */
/**
 * @brief  设置指定通道的输出频率，并使信号【立刻改变】。
 * @param  channel: 目标通道号 (AD9959_CHANNEL_0 ~ 3 或 CHANNEL_ALL)
 * @param  freq:    目标基波输出频率值，单位: 赫兹 (Hz)
 */
void AD9959_SetFrequency(uint8_t channel, uint32_t freq);

/**
 * @brief  设置指定通道的输出频率，但【暂时不输出改变】，需等后续手动调用 Update 改变。
 * @note   常用于需要多通道保持绝对相位对齐同步变频的场景。
 */
void AD9959_SetFrequencyNoUpdate(uint8_t channel, uint32_t freq);

/* ---- 幅度设置核心接口 ---- */
/**
 * @brief  设置指定通道的输出幅值大小，并【立刻改变】。
 * @param  channel:   目标通道号
 * @param  amplitude: 期望的输出幅值电压，单位: 毫伏 (mV)，范围 0 ~ 576mV
 */
void AD9959_SetAmplitude(uint8_t channel, uint16_t amplitude);

/**
 * @brief  设置指定通道的输出幅值大小，但【暂不生效】。
 */
void AD9959_SetAmplitudeNoUpdate(uint8_t channel, uint16_t amplitude);

/* ---- 相位设置核心接口 ---- */
/**
 * @brief  设置指定通道信号的初始相位，并【立刻改变】。
 * @param  channel: 目标通道号
 * @param  phase:   期望的初始相位角，单位: 度 (°)，范围 0 ~ 360
 */
void AD9959_SetPhase(uint8_t channel, uint16_t phase);

/**
 * @brief  设置指定通道信号的初始相位，但【暂不生效】。
 */
void AD9959_SetPhaseNoUpdate(uint8_t channel, uint16_t phase);

/* ========================================================================== */
/* 8. 历史存留 API 接口 (Legacy API)                                           */
/* 在内部完全通过“披着羊皮的外壳”调用标准 API，作用是防止旧工程的老应用层代码报错崩溃。 */
/* ========================================================================== */

void delay1(uint32_t length); /**< 简单的微秒级阻塞延时函数 */
void IntReset(void);          /**< 兼容旧命名：硬件复位函数 */
void IO_Update(void);         /**< 兼容旧命名：引脚数据更新发送函数 */
void Intserve(void);          /**< 兼容旧命名：总线引脚设为空闲 */
void Init_AD9959(void);       /**< 兼容旧命名：初始化 AD9959 驱动系统 */

/** @brief 兼容旧命名：向 AD9959 寄存器写入多字节数据的外壳接口 */
void WriteData_AD9959(uint8_t RegisterAddress, uint8_t NumberofRegisters,
                      uint8_t *RegisterData, uint8_t temp);

void Write_frequence(uint8_t Channel, uint32_t Freq);                 /**< 旧命名：设置频率并同步更新 */
void Write_frequence_no_update(uint8_t Channel, uint32_t Freq);       /**< 旧命名：设置频率，暂不更新 */
void Write_Amplitude(uint8_t Channel, uint16_t Ampli);                /**< 旧命名：设置幅值并同步更新 */
void Write_Amplitude_no_update(uint8_t Channel, uint16_t Ampli);      /**< 旧命名：设置幅值，暂不更新 */
void Write_Phase(uint8_t Channel, uint16_t Phase);                    /**< 旧命名：设置相位并同步更新 */
void Write_Phase_no_update(uint8_t Channel, uint16_t Phase);          /**< 旧命名：设置相位，暂不更新 */

#ifdef __cplusplus
}
#endif

#endif /* __AD9959_H */
