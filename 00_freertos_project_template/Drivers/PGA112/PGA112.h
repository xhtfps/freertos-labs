/**
 * @file Drivers/PGA112/PGA112.h
 * @brief PGA112 可编程增益放大器设备对象模块。
 * @details
 * - 属于 PGA112 增益控制链路，分离设备语义和软件 SPI 总线时序。
 * - 测量服务只关心增益码和通道码，底层帧格式与 GPIO 翻转保留在驱动内部。
 * - 旧 API 保留为兼容层，新代码优先使用对象接口。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __PGA112_H
#define __PGA112_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* PGA112 SPI 通信命令字（16位）                                              */
/* ========================================================================== */
/* 注：PGA112 的 SPI 帧为 16 位，高 8 位为命令/操作码，低 8 位为数据/参数 */

#define PGA112_CMD_WRITE            0x2A00U   /* 写配置寄存器命令（用于设置增益和通道） */
#define PGA112_CMD_READ             0x6A00U   /* 读配置寄存器命令 */
#define PGA112_CMD_NOP              0x0000U   /* 空操作命令（NOP），通常在读取数据时用于提供时钟 */
#define PGA112_CMD_SDN_DIS          0xE100U   /* 退出停机模式命令（Shutdown Disable）-> 唤醒芯片 */
#define PGA112_CMD_SDN_EN           0xE1F1U   /* 进入停机模式命令（Shutdown Enable） -> 芯片进入超低功耗状态 */

/* ========================================================================== */
/* 输入通道选择代码（Channel Code）                                            */
/* ========================================================================== */
/* PGA112 拥有 2 组模拟输入通道（CH0, CH1）以及 4 组内部固定的校准/测试通道 */

#define PGA112_CHANNEL_CH0          0x00U     /* 模拟输入通道 0 */
#define PGA112_CHANNEL_CH1          0x01U     /* 模拟输入通道 1 */
#define PGA112_CHANNEL_CAL1         0x0CU     /* 内部校准通道 1：接入 GND */
#define PGA112_CHANNEL_CAL2         0x0DU     /* 内部校准通道 2：接入 0.9 * VREF */
#define PGA112_CHANNEL_CAL3         0x0EU     /* 内部校准通道 3：接入 0.1 * VREF */
#define PGA112_CHANNEL_CAL4         0x0FU     /* 内部校准通道 4：无缝连接 VREF */

/* ========================================================================== */
/* 可编程增益选择代码（Gain Code）                                             */
/* ========================================================================== */
/* 写入寄存器的 3 位代码与实际二进制倍率（1 到 128 倍）的映射关系 */

#define PGA112_GAIN_1               0x00U     /* 放大倍数：1 倍 */
#define PGA112_GAIN_2               0x01U     /* 放大倍数：2 倍 */
#define PGA112_GAIN_4               0x02U     /* 放大倍数：4 倍 */
#define PGA112_GAIN_8               0x03U     /* 放大倍数：8 倍 */
#define PGA112_GAIN_16              0x04U     /* 放大倍数：16 倍 */
#define PGA112_GAIN_32              0x05U     /* 放大倍数：32 倍 */
#define PGA112_GAIN_64              0x06U     /* 放大倍数：64 倍 */
#define PGA112_GAIN_128             0x07U     /* 放大倍数：128 倍 */

/* ========================================================================== */
/* 超声波应用默认业务配置                                                     */
/* ========================================================================== */

#define PGA112_ULTRASONIC_CHANNEL   PGA112_CHANNEL_CH0  /* 默认将超声波回波信号接在 CH0 通道 */
#define PGA112_DEFAULT_GAIN_CODE    PGA112_GAIN_8       /* 默认初始放大倍数设为 8 倍 */

typedef struct Pga112Device Pga112Device;

/**
 * @brief PGA112 设备对象操作表。
 * @details 该表表达“可编程增益放大器”的设备语义，底层软件 SPI 细节由 bus 依赖处理。
 */
typedef struct
{
    void (*init)(Pga112Device *self);                                                   /**< 初始化总线并写入默认增益/通道。 */
    void (*shutdown)(Pga112Device *self, FunctionalState new_state);                    /**< 控制 PGA112 进入或退出停机模式。 */
    void (*write_config)(Pga112Device *self, uint8_t gain_code, uint8_t channel_code);  /**< 同步写入增益码和输入通道码。 */
    void (*set_gain_code)(Pga112Device *self, uint8_t gain_code);                       /**< 只修改增益码，保持当前通道不变。 */
    void (*select_channel)(Pga112Device *self, uint8_t channel_code);                   /**< 只切换输入通道，保持当前增益不变。 */
    uint8_t (*get_gain_code)(Pga112Device *self);                                       /**< 返回对象缓存的当前增益码。 */
    uint8_t (*get_channel_code)(Pga112Device *self);                                    /**< 返回对象缓存的当前通道码。 */
    uint16_t (*get_gain_value)(Pga112Device *self, uint8_t gain_code);                  /**< 将增益码转换为实际倍率。 */
} Pga112DeviceOps;

struct SoftSpiBus;

/**
 * @brief PGA112 设备对象。
 * @details 保存设备语义状态，通过注入的软件 SPI 总线完成实际帧传输。
 */
struct Pga112Device
{
    struct SoftSpiBus *bus;      /**< 底层半双工软件 SPI 总线依赖。 */
    uint8_t gain_code;           /**< 当前缓存的增益码，范围 PGA112_GAIN_1 到 PGA112_GAIN_128。 */
    uint8_t channel_code;        /**< 当前缓存的输入通道码。 */
    const Pga112DeviceOps *ops;  /**< 对象方法表。 */
};

extern Pga112Device g_pga112_device;

/* ========================================================================== */
/* 外部函数声明                                                               */
/* ========================================================================== */

void PGA112_Init(void);                                         /* 初始化 PGA112（配置 SPI 并写入初始增益与通道） */
void PGA112_Shutdown(FunctionalState new_state);                /* 控制芯片进入(ENABLE)或退出(DISABLE)低功耗停机模式 */
void PGA112_WriteConfig(uint8_t gain_code, uint8_t channel_code); /* 同时写入新的增益代码和通道代码 */
void PGA112_SetGainCode(uint8_t gain_code);                     /* 快捷修改放大倍数（保持当前通道不变） */
void PGA112_SelectChannel(uint8_t channel_code);                 /* 快捷切换输入通道（保持当前放大倍数不变） */
uint8_t PGA112_GetGainCode(void);                               /* 获取当前单片机中缓存的增益代码 */
uint8_t PGA112_GetChannelCode(void);                            /* 获取当前单片机中缓存的通道代码 */
uint16_t PGA112_GetGainValue(uint8_t gain_code);                /* 将增益代码(0~7)换算为实际的倍数数字(1~128) */

#ifdef __cplusplus
}
#endif

#endif /* __PGA112_H */
