/**
 * @file Drivers/BSP/SRAM/sram.h
 * @brief 外部 SRAM/FSMC 驱动对象模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __SRAM_H
#define __SRAM_H

#include "./SYSTEM/sys/sys.h"


/******************************************************************************************/
/* SRAM WR/RD/CS 引脚 定义 
 * SRAM_D0~D15 及 地址线,由于引脚太多,就不在这里定义了,直接在SRAM_init里面修改.所以在移植的时候,
 * 除了改这3个IO口, 还得改SRAM_init里面的 数据线 和 地址线 所在的IO口.
 */

#define SRAM_WR_GPIO_PORT               GPIOD
#define SRAM_WR_GPIO_PIN                GPIO_PIN_5
#define SRAM_WR_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

#define SRAM_RD_GPIO_PORT               GPIOD
#define SRAM_RD_GPIO_PIN                GPIO_PIN_4
#define SRAM_RD_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

/* SRAM_CS(需要根据SRAM_FSMC_NEX设置正确的IO口) 引脚 定义 */
#define SRAM_CS_GPIO_PORT                GPIOG
#define SRAM_CS_GPIO_PIN                 GPIO_PIN_10
#define SRAM_CS_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOG_CLK_ENABLE(); }while(0)   /* 所在IO口时钟使能 */

/* FSMC相关参数 定义 
 * 注意: 我们默认是通过FSMC块3来连接SRAM, 块1有4个片选: FSMC_NE1~4
 *
 * 修改SRAM_FSMC_NEX, 对应的SRAM_CS_GPIO相关设置也得改
 */
#define SRAM_FSMC_NEX           3         /* 使用FSMC_NE3接SRAM_CS,取值范围只能是: 1~4 */

#define SRAM_FSMC_BCRX          FSMC_Bank1->BTCR[(SRAM_FSMC_NEX - 1) * 2]       /* BCR寄存器,根据SRAM_FSMC_NEX自动计算 */
#define SRAM_FSMC_BTRX          FSMC_Bank1->BTCR[(SRAM_FSMC_NEX - 1) * 2 + 1]   /* BTR寄存器,根据SRAM_FSMC_NEX自动计算 */
#define SRAM_FSMC_BWTRX         FSMC_Bank1E->BWTR[(SRAM_FSMC_NEX - 1) * 2]      /* BWTR寄存器,根据SRAM_FSMC_NEX自动计算 */

/******************************************************************************************/

/* SRAM基地址, 根据 SRAM_FSMC_NEX 的设置来决定基址地址
 * 我们一般使用FSMC的块1(BANK1)来驱动SRAM, 块1地址范围总大小为256MB,均分成4块:
 * 存储块1(FSMC_NE1)地址范围: 0X6000 0000 ~ 0X63FF FFFF
 * 存储块2(FSMC_NE2)地址范围: 0X6400 0000 ~ 0X67FF FFFF
 * 存储块3(FSMC_NE3)地址范围: 0X6800 0000 ~ 0X6BFF FFFF
 * 存储块4(FSMC_NE4)地址范围: 0X6C00 0000 ~ 0X6FFF FFFF
 */
#define SRAM_BASE_ADDR         (0X60000000 + (0X4000000 * (SRAM_FSMC_NEX - 1)))

extern SRAM_HandleTypeDef g_sram_handler;    /* SRAM句柄 */

typedef struct SramDevice SramDevice;

/**
 * @brief 外部 SRAM 设备操作表。
 */
typedef struct
{
    void (*init)(SramDevice *self);                                                    /**< 初始化 FSMC/SRAM 时序。 */
    void (*write)(SramDevice *self, uint8_t *pbuf, uint32_t addr, uint32_t datalen);    /**< 按偏移地址连续写入。 */
    void (*read)(SramDevice *self, uint8_t *pbuf, uint32_t addr, uint32_t datalen);     /**< 按偏移地址连续读取。 */
    void (*test_write)(SramDevice *self, uint32_t addr, uint8_t data);                  /**< 测试写一个字节。 */
    uint8_t (*test_read)(SramDevice *self, uint32_t addr);                              /**< 测试读一个字节。 */
} SramDeviceOps;

/**
 * @brief 外部 SRAM 设备对象。
 */
struct SramDevice
{
    SRAM_HandleTypeDef *handle; /**< HAL SRAM/FSMC 句柄。 */
    uint32_t base_addr;         /**< 外部 SRAM 映射到 MCU 地址空间的基地址。 */
    uint8_t fsmc_nex;           /**< FSMC 片选编号。 */
    const SramDeviceOps *ops;   /**< SRAM 方法表。 */
};

extern SramDevice g_external_sram;

void sram_init(void);
void sram_write(uint8_t *pbuf, uint32_t addr, uint32_t datalen);
void sram_read(uint8_t *pbuf, uint32_t addr, uint32_t datalen);

uint8_t sram_test_read(uint32_t addr);
void sram_test_write(uint32_t addr, uint8_t data);

#endif
