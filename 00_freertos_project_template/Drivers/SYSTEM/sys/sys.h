/**
 * @file Drivers/SYSTEM/sys/sys.h
 * @brief 系统时钟、NVIC、低功耗和芯片级控制模块。
 * @details
 * - 属于系统基础服务层，封装时钟、延时、串口或芯片级公共能力。
 * - 上层业务依赖稳定接口，不复制 HAL 初始化细节。
 * - 修改系统级参数时需要同步考虑 FreeRTOS 调度和中断优先级约束。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef _SYS_H
#define _SYS_H

#include "stm32f4xx.h"
#include "core_cm4.h"
#include "stm32f4xx_hal.h"


/**
 * SYS_SUPPORT_OS用于定义系统文件夹是否支持OS
 * 0,不支持OS
 * 1,支持OS
 */
#define SYS_SUPPORT_OS         1


/*函数声明*******************************************************************************************/

typedef struct SystemController SystemController;

/**
 * @brief 芯片级系统控制操作表。
 * @details 将启动、时钟、复位和中断屏蔽等系统能力集中到一个对象接口中。
 */
typedef struct
{
    void (*set_vector_table)(SystemController *self, uint32_t baseaddr, uint32_t offset); /**< 配置中断向量表基址和偏移。 */
    void (*standby)(SystemController *self);                                             /**< 进入待机低功耗模式。 */
    void (*soft_reset)(SystemController *self);                                          /**< 触发系统软件复位。 */
    uint8_t (*clock_init)(SystemController *self, uint32_t plln, uint32_t pllm,
                          uint32_t pllp, uint32_t pllq);                                 /**< 配置主 PLL 和系统时钟树。 */
    void (*wfi)(SystemController *self);                                                 /**< 执行 WFI 指令等待中断唤醒。 */
    void (*disable_irq)(SystemController *self);                                         /**< 全局关闭可屏蔽中断。 */
    void (*enable_irq)(SystemController *self);                                          /**< 全局开启可屏蔽中断。 */
    void (*set_msp)(SystemController *self, uint32_t addr);                              /**< 设置主栈指针 MSP。 */
} SystemControllerOps;

/**
 * @brief 系统控制器对象。
 * @note 当前对象无额外状态，保留结构体是为了统一 ops/self 调用风格。
 */
struct SystemController
{
    const SystemControllerOps *ops; /**< 系统控制方法表。 */
};

extern SystemController g_system_controller;

void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset);                         /* 设置中断偏移量 */
void sys_standby(void);                                                                     /* 进入待机模式 */
void sys_soft_reset(void);                                                                  /* 系统软复位 */
uint8_t sys_stm32_clock_init(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq);   /* 配置系统时钟 */


/* 以下为汇编函数 */
void sys_wfi_set(void);             /* 执行WFI指令 */
void sys_intx_disable(void);        /* 关闭所有中断 */
void sys_intx_enable(void);         /* 开启所有中断 */
void sys_msr_msp(uint32_t addr);    /* 设置栈顶地址 */

#endif

