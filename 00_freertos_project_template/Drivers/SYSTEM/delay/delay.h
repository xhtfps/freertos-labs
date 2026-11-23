/**
 * @file Drivers/SYSTEM/delay/delay.h
 * @brief 系统阻塞延时服务模块。
 * @details
 * - 属于系统基础服务层，封装时钟、延时、串口或芯片级公共能力。
 * - 上层业务依赖稳定接口，不复制 HAL 初始化细节。
 * - 修改系统级参数时需要同步考虑 FreeRTOS 调度和中断优先级约束。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __DELAY_H
#define __DELAY_H

#include "./SYSTEM/sys/sys.h"

typedef struct DelayService DelayService;

/**
 * @brief 阻塞延时服务操作表。
 */
typedef struct
{
    void (*init)(DelayService *self, uint16_t sysclk); /**< 根据系统主频计算微秒延时系数。 */
    void (*us)(DelayService *self, uint32_t nus);      /**< 阻塞延时指定微秒数。 */
    void (*ms)(DelayService *self, uint16_t nms);      /**< 阻塞延时指定毫秒数。 */
} DelayServiceOps;

/**
 * @brief 阻塞延时服务对象。
 */
struct DelayService
{
    uint32_t fac_us;             /**< 每微秒对应的 SysTick/内核计数系数。 */
    const DelayServiceOps *ops;  /**< 延时服务方法表。 */
};

extern DelayService g_delay_service;

void delay_init(uint16_t sysclk);           /* 初始化延迟函数 */
void delay_ms(uint16_t nms);                /* 延时nms */
void delay_us(uint32_t nus);                /* 延时nus */

#if (!SYS_SUPPORT_OS)                       /* 没有使用Systick中断 */
    void HAL_Delay(uint32_t Delay);         /* HAL库的延时函数，SDIO等需要用到 */
#endif

#endif

