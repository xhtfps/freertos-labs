/**
 * @file User/stm32f4xx_it.h
 * @brief Cortex-M4 系统异常与中断入口声明。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

/* 头文件保护：该文件由启动文件和系统异常入口共同依赖。 */
#ifndef __STM32F4xx_IT_H
#define __STM32F4xx_IT_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* 系统异常入口声明。
 * 这些函数名由启动文件中的中断向量表引用，命名不可随意修改。
 * FreeRTOS 接管的入口会在 FreeRTOSConfig.h 中通过宏映射到内核实现。
 */
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_IT_H */
