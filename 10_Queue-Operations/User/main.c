#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/PS2/ps2.h"
#include "./BSP/SRAM/sram.h"
#include "./MALLOC/malloc.h"
#include "freertos_demo.h"
#include "./BSP/TIMER/btim.h"

int main(void)
{
    HAL_Init();                         /* 初始化HAL库 */
    sys_stm32_clock_init(336, 8, 2, 7); /* 设置时钟,168Mhz */
    delay_init(168);                    /* 延时初始化 */
    usart_init(115200);                 /* 串口初始化为115200 */
    led_init();                         /* 初始化LED */
    lcd_init();                         /* 初始化LCD */
    sram_init();                        /* SRAM初始化 */
    PS2_Keyboard_Init();                /* PS2键盘初始化 */
    btim_tim4_int_init(1000 - 1, 72 - 1);		/* TIM4初始化，1ms中断，用于PS2键盘状态机扫描 */					
//	btim_timx_int_init(10000 - 1, 8400 - 1);		/* 1s中断一次 */	
//	btim_timx4_int_init(10000 - 1, 8400 - 1);		/* 1s中断一次 */
	
    my_mem_init(SRAMIN);                /* 初始化内部SRAM内存池 */
    my_mem_init(SRAMEX);                /* 初始化外部SRAM内存池 */
    my_mem_init(SRAMCCM);               /* 初始化内部CCM内存池 */
    
    freertos_demo();                    /* 运行FreeRTOS例程 */
}
