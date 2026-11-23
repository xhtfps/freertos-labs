/**
 * @file User/main.c
 * @brief 系统启动入口与板级初始化编排。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./TFT_LCD/TFT_LCD.h"
#include "./TFT_LCD/fontupd.h"
#include "./BSP/PS2/ps2.h"
#include "./BSP/SRAM/sram.h"
#include "./MALLOC/malloc.h"
#include "freertos_demo.h"

/**
 * @brief  应用程序主入口点
 * @details 负责整个系统的硬件初始化、内存堆栈划分，并最终引导至 FreeRTOS 操作系统
 * @param  无
 * @retval int (通常在嵌入式 RTOS 项目中主函数为死循环，不会返回)
 */
int main(void)
{
    // 1. 初始化 STM32 HAL 库核心外设（复位所有外设、配置 Flash 接口及 Systick）
    HAL_Init();
    
    // 2. 配置系统时钟树 (以 STM32F407/429 为例)
    // 参数含义通常为：VCO倍频系数=336, PLL分频因子M=8, 主系统时钟分频因子P=2, USB/SDIO时钟分频因子Q=7
    // 由此配置计算出主频：(8MHz / 8) * 336 / 2 = 168MHz
    sys_stm32_clock_init(336, 8, 2, 7);
    
    // 3. 初始化精确延时函数组件，传入当前系统主频 168MHz 供内核计数器匹配
    delay_init(168);
    
    // 4. 初始化调试串口 USART1，配置波特率为 115200bps，用于 printf 打印调试信息
    usart_init(115200);
    
    // 5. 初始化板载 LED 灯的 GPIO 引脚
    led_init();
    
    // 6. 初始化 TFT 液晶显示屏驱动（配置相关 FSMC 接口或普通 GPIO 并加载 LCD 寄存器参数）
    TFT_LCD_Init();
    LCD_SetColors(White, Black);
    if (font_init())
    {
        LCD_DisplayStringLine(200, 400, (uint8_t *)"Font Error!!!");
    }
    
    // 7. 短暂延时 10 毫秒，等待液晶屏及其它硬件上电电平稳定
    delay_ms(10);
    
    // 8. 初始化外部 SRAM（配置 FSMC 静态存储控制器，扩展 MCU 的运行内存空间）
    sram_init();
    
    // 9. 初始化 PS/2 键盘接口（配置相关的时钟和数据通信引脚）
    PS2_Keyboard_Init();

    /* ==================================================================================
     * 10. 内存管理分配器 (Malloc) 初始化
     * ==================================================================================
     * 对 MCU 可用的三块独立物理内存池进行管理结构体初始化，方便后续调用 mymalloc 分配内存。
     */
    my_mem_init(SRAMIN);  // 初始化内部 SRAM 内存池 (通常为芯片自带的 128KB 左右空间)
    my_mem_init(SRAMEX);  // 初始化外部 SRAM 内存池 (即刚刚通过 sram_init 挂载的板载几兆外部芯片)
    my_mem_init(SRAMCCM); // 初始化 CCM (Core Coupled Memory) 内存池 (内核紧耦合内存，仅供 CPU 访问，速度极快)

    /* ==================================================================================
     * 11. 启动 FreeRTOS 操作系统演示程序
     * ==================================================================================
     * 该函数内部会创建“起始/创建任务 (Start Task)”，随后调用 vTaskStartScheduler() 启动内核调度器。
     * 程序一旦进入调度器，将由 RTOS 依照优先级接管并轮询所有任务，main 函数此行以下的代码将不再被执行。
     */
    freertos_demo();
}
