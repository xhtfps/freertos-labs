/**
 * @file User/stm32f4xx_it.c
 * @brief Cortex-M4 系统异常与默认中断入口实现。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "stm32f4xx_it.h"
#include "stm32f4xx_hal.h"
#include "./SYSTEM/sys/sys.h"

/* Cortex-M4 系统异常处理入口。
 * 这些函数运行在异常上下文中，默认只保留最小处理逻辑。
 * 故障类异常进入死循环，便于连接调试器后查看堆栈和寄存器。
 */

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
#if (!SYS_SUPPORT_OS)
void SVC_Handler(void)
{
}
#endif

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
#if (!SYS_SUPPORT_OS)
void PendSV_Handler(void)
{
}
#endif

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
#if (!SYS_SUPPORT_OS)
void SysTick_Handler(void)
{
  HAL_IncTick();
}
#endif

/* 外设中断说明：
 * 具体外设中断应优先放在所属驱动模块内，并在 ISR 中快速转发到对象方法。
 * 本文件只保留系统公共异常入口，避免形成新的“大杂烩”中断文件。
 */

