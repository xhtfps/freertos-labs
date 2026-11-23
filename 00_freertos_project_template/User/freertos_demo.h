/**
 * @file User/freertos_demo.h
 * @brief FreeRTOS 启动模块对外接口。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __FREERTOS_DEMO_H
#define __FREERTOS_DEMO_H

#include <stdint.h>

/**
 * @brief  FreeRTOS 演示/测试主入口函数
 * @note   通常在 main.c 中调用，用于初始化 FreeRTOS 相关任务、队列及信号量
 * @retval 无
 */
void freertos_demo(void);

/**
 * @brief  进入睡眠模式前的预处理函数
 * @note   当 FreeRTOS 开启 Tickless 模式（configUSE_TICKLESS_IDLE == 1）时，
 * 在系统进入低功耗睡眠模式前，内核会自动调用此函数。
 * @param  expected_idle_time: 指向期望空闲时间的指针（单位：系统时钟节拍 Tick）。
 * 可以在此函数内根据需要修改该值，或者在此处关闭外设时钟、
 * 降低系统主频、配置唤醒源等。
 * @retval 无
 */
void PRE_SLEEP_PROCESSING(uint32_t *expected_idle_time);

/**
 * @brief  退出睡眠模式后的后处理函数
 * @note   当系统被外部中断（如定时器、按键、外设中断）唤醒，退出低功耗模式时，
 * 内核会自动首先调用此函数。
 * @note   通常用于在此处恢复系统时钟、重新初始化或开启在睡眠前关闭的外设，
 * 以确保后续任务能正常运行。
 * @retval 无
 */
void POST_SLEEP_PROCESSING(void);

#endif /* __FREERTOS_DEMO_H */
