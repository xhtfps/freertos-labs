#include "freertos_demo.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/PS2/ps2.h"
#include "./SYSTEM/delay/delay.h"
#include "./MALLOC/malloc.h"

/*FreeRTOS*********************************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/******************************************************************************************************/
/*FreeRTOS配置*/

/* START_TASK 任务配置
 * 优先级: 1 堆栈大小:128 任务句柄:start_task_handler
 */
#define START_TASK_PRIO			1
#define START_TASK_STACK_SIZE 	128
TaskHandle_t	start_task_handler;
void start_task(void * pvParameters);

/* TASK1 任务配置
 * 优先级: 2 堆栈大小:128 任务句柄:task1_handler
 */
#define TASK1_PRIO				2
#define TASK1_STACK_SIZE 		128
TaskHandle_t	task1_handler;
void task1(void * pvParameters);

/* TASK2 任务配置
 * 优先级: 3 堆栈大小:128 任务句柄:task2_handler
 */
#define TASK2_PRIO				3
#define TASK2_STACK_SIZE 		128
TaskHandle_t	task2_handler;
void task2(void * pvParameters);

/* TASK3 任务配置
 * 优先级: 4 堆栈大小:128 任务句柄:task3_handler
 */
#define TASK3_PRIO				4
#define TASK3_STACK_SIZE 		128
TaskHandle_t	task3_handler;
void task3(void * pvParameters);
/******************************************************************************************************/
QueueHandle_t ps2_key_queue;			/* 小数据队列句柄 */
QueueHandle_t big_data_queue;		/* 大数据队列句柄 */
char buff[200] = {"落魄谷中寒风吹，春秋蝉鸣少年归。\r\n荡魂山处石人泪，定仙游走魔向北。\r\n逆流河上万仙退，爱情不敌坚持泪。\r\n宿命天成命中败，仙尊悔而我不悔。"};	/* 大数组 */

/**
 * @brief       FreeRTOS 任务创建
 * @param       无
 * @retval      无
 */
void freertos_demo(void)
{
	/* 创建队列实现 */
	ps2_key_queue = xQueueCreate(2, sizeof(uint8_t));			/* 创建一个长度为2，数据大小为unsigned char的队列 */
	if(ps2_key_queue != NULL)
	{
		printf("ps2_key_queue队列创建成功\n");
	}
	else
	{
		printf("ps2_key_queue队列创建失败\r\n");
	}

	big_data_queue = xQueueCreate(1, sizeof(char*));		/* 创建一个长度为1，数据大小为unsigned char的队列 */
	if(big_data_queue != NULL)
	{
		printf("big_data_queue队列创建成功\r\n");
	}
	else
	{
		printf("big_data_queue队列创建失败\r\n");
	}

	xTaskCreate ((TaskFunction_t		) start_task,
				(char *					) "start_task",
				(configSTACK_DEPTH_TYPE	) START_TASK_STACK_SIZE,
				(void * 				) NULL,
				(UBaseType_t			) START_TASK_PRIO,
				(TaskHandle_t *			) &start_task_handler );
	vTaskStartScheduler();
}

void start_task(void * pvParameters)
{
	taskENTER_CRITICAL();		/* 进入临界区 */
	xTaskCreate ((TaskFunction_t		) task1,
				(char *					) "task1",
				(configSTACK_DEPTH_TYPE	) TASK1_STACK_SIZE,
				(void * 				) NULL,
				(UBaseType_t			) TASK1_PRIO,
				(TaskHandle_t *			) &task1_handler );

	xTaskCreate ((TaskFunction_t		) task2,
				(char *					) "task2",
				(configSTACK_DEPTH_TYPE	) TASK2_STACK_SIZE,
				(void * 				) NULL,
				(UBaseType_t			) TASK2_PRIO,
				(TaskHandle_t *			) &task2_handler );

	xTaskCreate ((TaskFunction_t		) task3,
				(char *					) "task3",
				(configSTACK_DEPTH_TYPE	) TASK3_STACK_SIZE,
				(void * 				) NULL,
				(UBaseType_t			) TASK3_PRIO,
				(TaskHandle_t *			) &task3_handler );

	vTaskDelete(NULL);
	taskEXIT_CRITICAL();		/* 退出临界区 */
}

/* 任务1: 实现入队 */
/* 任务1: 实现入队 */
void task1(void * pvParameters)
{
	uint8_t ps2_key = 0;
	BaseType_t ret = 0;
	char *buf = buff;	/*等价于char *buf = &buff[0]; */
	while(1)
	{
		/* PS2键盘扫描 */
		PS2_KeyScan();
		ps2_key = PS2_GetCurrentKeyValue();
        
		if (ps2_key != PS2_KEY_VALUE_NULL)
		{
			printf("PS2按键值: %d\r\n", ps2_key);
            if(ps2_key == PS2_KEY_VALUE_1 || ps2_key == PS2_KEY_VALUE_2)
			{
				ret = xQueueSend(ps2_key_queue, &ps2_key, portMAX_DELAY);
				if(ret != pdTRUE)
				{
					printf("ps2_key_queue写入队列失败\r\n");
				}
			}
			if(ps2_key == PS2_KEY_VALUE_3)
			{
				ret = xQueueSend(big_data_queue, &buf, portMAX_DELAY);
				if(ret != pdTRUE)
				{
					printf("big_data_queue写入队列失败\r\n");
				}
			}
		}

		vTaskDelay(10);
	}
}

/* 任务2:实现小数据出队 */
void task2(void * pvParameters)
{
	uint8_t ps2_key = 0;
	BaseType_t ret = 0;
	while(1)
	{
		ret = xQueueReceive(ps2_key_queue, &ps2_key, portMAX_DELAY);	/* 从ps2_key_queue队列接收数据 */
		if(ret == pdTRUE)
		{
			printf("ps2_key_queue读取队列成功，接收到按键值: %d\r\n", ps2_key);
		}
		else
		{
			printf("ps2_key_queue读取队列失败\r\n");
		}
	}
}


/* 任务3: 实现大数据出队 */
void task3(void * pvParameters)
{
	char *buf = NULL;
	BaseType_t ret = 0;
	while(1)
	{
		ret = xQueueReceive(big_data_queue, &buf, portMAX_DELAY);	/* 从big_data_queue队列接收数据 */
		if(ret == pdTRUE)
		{
			printf("big_data_queue读取队列成功，接收到大数组为:\r\n%s\r\n", buf);
		}
		else
		{
			printf("big_data_queue读取队列失败\r\n");
		}
	}
}
