/**
 * @file Drivers/BSP/TIMER/ultrasonic_pwm.c
 * @brief 超声发射 PWM 脉冲串对象模块。
 * @details
 * - 属于定时器子系统，封装基础计数、PWM、输入捕获或超声专用时序。
 * - 业务层通过对象方法或兼容 API 使用定时器能力，不直接散落 TIM 寄存器配置。
 * - 中断处理保持短小，只维护必要状态并把复杂逻辑交给任务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/TIMER/ultrasonic_timer.h"
#include "./BSP/TIMER/ultrasonic_pwm.h"

#include "FreeRTOS.h"
#include "task.h"

/** @brief STM32 HAL库定时器句柄，专门用于管理超声波发射的 PWM 时序 */
TIM_HandleTypeDef g_ultrasonic_pwm_tim;

/* =========================================================================
 * 内部私有方法前置声明
 * ========================================================================= */
static void UltrasonicPwm_Init(UltrasonicPwm *self);
static void UltrasonicPwm_Enable(UltrasonicPwm *self);
static void UltrasonicPwm_Disable(UltrasonicPwm *self);
static void UltrasonicPwm_FireBurst(UltrasonicPwm *self);
static uint8_t UltrasonicPwm_IsActive(UltrasonicPwm *self);
static void UltrasonicPwm_HandleIrq(UltrasonicPwm *self);
static void UltrasonicPwm_GpioInit(void);
static void UltrasonicPwm_FinishFromISR(UltrasonicPwm *self);

/* =========================================================================
 * 虚函数表定义 (V-Table)
 * ========================================================================= */
static const UltrasonicPwmOps s_ultrasonic_pwm_ops =
{
    UltrasonicPwm_Init,
    UltrasonicPwm_Enable,
    UltrasonicPwm_Disable,
    UltrasonicPwm_FireBurst,
    UltrasonicPwm_IsActive,
    UltrasonicPwm_HandleIrq
};

/** @brief 全局唯一的超声波 PWM 发射控制对象实例 */
UltrasonicPwm g_ultrasonic_pwm =
{
    &g_ultrasonic_pwm_tim,  /* TIM 句柄指针 */
    0U,                     /* cycle_count */
    0U,                     /* burst_pending */
    0U,                     /* active */
    &s_ultrasonic_pwm_ops   /* 绑定操作接口 */
};

/* =========================================================================
 * 对象私有方法实现
 * ========================================================================= */

/**
 * @brief 初始化超声波发射定时器及两路 PWM 通道
 * @details 
 * - 配置时基：向上计数模式，周期由 `ULTRASONIC_PWM_PERIOD_TICKS` 决定（通常为 40kHz）。
 * - 配置通道：CH1 设置为 PWM1 模式，CH2 设置为 PWM2 模式。这种配置多用于全桥或半桥
 * 差分驱动电路，能有效将探头两端的驱动电压提升一倍，增强发射声功率。
 * - 默认安全状态：关闭发射侧模拟开关，开启接收侧模拟开关，防止系统浮空产生噪声。
 * @param self 指向超声波 PWM 实例
 */
static void UltrasonicPwm_Init(UltrasonicPwm *self)
{
    TIM_OC_InitTypeDef tim_oc = {0};

    if ((self == NULL) || (self->handle == NULL))
    {
        return;
    }

    /* 1. 初始化底层的引脚复用和时钟 */
    UltrasonicPwm_GpioInit();
    __HAL_RCC_TIM4_CLK_ENABLE();

    /* 2. 配置定时器基础时基 */
    self->handle->Instance = ULTRASONIC_PWM_TIM;
    self->handle->Init.Prescaler = 0U; /* 预分频为 0，以获得最高的时间分辨率 */
    self->handle->Init.CounterMode = TIM_COUNTERMODE_UP;
    self->handle->Init.Period = ULTRASONIC_PWM_PERIOD_TICKS - 1U;
    self->handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    self->handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(self->handle);

    /* 3. 配置通道 1: PWM1 模式 (CNT < CCR 时为有效电平) */
    tim_oc.OCMode = TIM_OCMODE_PWM1;
    tim_oc.Pulse = ULTRASONIC_PWM_PULSE_TICKS; /* 占空比控制（通常 50%） */
    tim_oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    tim_oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(self->handle, &tim_oc, TIM_CHANNEL_1);

    /* 4. 配置通道 2: PWM2 模式 (CNT < CCR 时为无效电平)
     * 此时 CH1 和 CH2 输出波形天然反相，形成差分驱动，使探头两端电压翻倍
     */
    tim_oc.OCMode = TIM_OCMODE_PWM2;
    HAL_TIM_PWM_ConfigChannel(self->handle, &tim_oc, TIM_CHANNEL_2);

    /* 5. 配置并使能定时器中断 (优先级设为 5，高于或等于 FreeRTOS 可管理的最高中断) */
    HAL_NVIC_SetPriority(ULTRASONIC_PWM_IRQn, 5, 1);
    HAL_NVIC_EnableIRQ(ULTRASONIC_PWM_IRQn);

    /* 6. 初始化硬件收发逻辑门：保护接收硬件不被高压击穿 */
    ULTRASONIC_TX_CMP_OFF(); /* 确保发射模拟通道关闭 */
    ULTRASONIC_RX_CMP_ON();  /* 保持接收模拟通道开启 */
}

/**
 * @brief 纯手动开启 PWM 持续发射 (常用于持续波测相或硬件调试)
 * @details 此函数不限制脉冲个数，会一直输出脉冲，直到手动调用 Disable。
 */
static void UltrasonicPwm_Enable(UltrasonicPwm *self)
{
    if ((self == NULL) || (self->handle == NULL))
    {
        return;
    }

    self->burst_pending = 0U; /* 非有限脉冲串模式 */
    self->active = 1U;

    /* T/R 切换：立刻关闭接收，开启发射 */
    ULTRASONIC_RX_CMP_OFF();
    ULTRASONIC_TX_CMP_ON();

    /* 清空计数器并启动波形输出 */
    __HAL_TIM_SET_COUNTER(self->handle, 0U);
    __HAL_TIM_CLEAR_FLAG(self->handle, TIM_FLAG_UPDATE);
    HAL_TIM_PWM_Start(self->handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(self->handle, TIM_CHANNEL_2);
}

/**
 * @brief 强制关闭 PWM 输出并复位收发硬件控制门
 * @details 无论是连续模式还是脉冲串模式，调用此函数均会立即切断发射，还原接收通道。
 */
static void UltrasonicPwm_Disable(UltrasonicPwm *self)
{
    if ((self == NULL) || (self->handle == NULL))
    {
        return;
    }

    /* 停止硬件 PWM 发生和中断 */
    HAL_TIM_PWM_Stop(self->handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(self->handle, TIM_CHANNEL_2);
    __HAL_TIM_DISABLE_IT(self->handle, TIM_IT_UPDATE);
    __HAL_TIM_CLEAR_FLAG(self->handle, TIM_FLAG_UPDATE);

    /* 重置状态变量 */
    self->burst_pending = 0U;
    self->active = 0U;
    self->cycle_count = 0U;

    /* 恢复安全探头状态：关闭发射，启动接收，开始等待回波 */
    ULTRASONIC_TX_CMP_OFF();
    ULTRASONIC_RX_CMP_ON();
}

/**
 * @brief 触发一次精准数量的超声波脉冲串 (Burst Tx)
 * @details 
 * 这是超声波时差测距的核心控时函数。
 * 1. 在临界区内安全地设置 `burst_pending` 标志和计数器。
 * 2. 闭合发射大功率通路，断开接收保护通路。
 * 3. 开启硬件中断 `TIM_IT_UPDATE`。每次定时器溢出（完成一个周期脉冲）都会进一次中断自增计数。
 * 4. 最终通过系统的阻塞延迟 `vTaskDelay` 让出 CPU 权限，让脉冲在后台中断中自动计数发射。
 */
static void UltrasonicPwm_FireBurst(UltrasonicPwm *self)
{
    if ((self == NULL) || (self->handle == NULL))
    {
        return;
    }

    /* 进入 FreeRTOS 临界区，防止多任务并发修改发射状态 */
    taskENTER_CRITICAL();
    self->cycle_count = 0U;
    self->burst_pending = 1U; /* 激活脉冲数控制逻辑 */
    self->active = 1U;
    taskEXIT_CRITICAL();

    /* 切换硬件收发门 */
    ULTRASONIC_RX_CMP_OFF();
    ULTRASONIC_TX_CMP_ON();

    /* 同步清空硬件计数器，避免第一枚脉冲出现周期畸变 */
    __HAL_TIM_SET_COUNTER(self->handle, 0U);
    __HAL_TIM_CLEAR_FLAG(self->handle, TIM_FLAG_UPDATE);

    /* 启动差分波形发生器，并挂载溢出中断 */
    HAL_TIM_PWM_Start(self->handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(self->handle, TIM_CHANNEL_2);
    __HAL_TIM_ENABLE_IT(self->handle, TIM_IT_UPDATE);

    /* 阻塞 1ms。由于超声波发射通常只需要几个到十几个周期（40kHz下 10个脉冲仅需250微秒），
     * 1ms 的延迟足以确保后台中断完全释放完毕，同时能释放 CPU 给其他低优先级任务。
     */
    vTaskDelay(pdMS_TO_TICKS(1U));
}

/**
 * @brief 查询当前超声波发射模块是否正在工作（是否忙）
 * @return 1: 正在发射脉冲; 0: 处于空闲或接收回波状态
 */
static uint8_t UltrasonicPwm_IsActive(UltrasonicPwm *self)
{
    if (self == NULL)
    {
        return 0U;
    }

    return self->active;
}

/**
 * @brief 定时器溢出中断服务实体 (ISR 核心)
 * @details 
 * 每当一个完整的 PWM 周期发射完毕，定时器触发 UPDATE 中断。
 * 在中断内，代码对已发射的脉冲数进行累加。一旦累加值达到设定的 `ULTRASONIC_BURST_CYCLES`，
 * 说明一整个脉冲串已发射完毕，立即调用 `UltrasonicPwm_FinishFromISR` 在硬件级切断发射，
 * 并瞬间开启接收，这最大化地缩短了硬件盲区。
 */
static void UltrasonicPwm_HandleIrq(UltrasonicPwm *self)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if ((self == NULL) || (self->handle == NULL))
    {
        portYIELD_FROM_ISR(higher_priority_task_woken);
        return;
    }

    /* 确认是硬件溢出中断，且软件确实使能了该中断 */
    if ((__HAL_TIM_GET_FLAG(self->handle, TIM_FLAG_UPDATE) != RESET) &&
        (__HAL_TIM_GET_IT_SOURCE(self->handle, TIM_IT_UPDATE) != RESET))
    {
        /* 及时清除中断标志位，防止死循环 */
        __HAL_TIM_CLEAR_IT(self->handle, TIM_IT_UPDATE);

        /* 处于有限脉冲串控制状态 */
        if (self->burst_pending != 0U)
        {
            self->cycle_count++;
            /* 达到预设的发射脉冲个数 (例如标准的 8 个脉冲) */
            if (self->cycle_count >= ULTRASONIC_BURST_CYCLES)
            {
                UltrasonicPwm_FinishFromISR(self);
            }
        }
    }

    /* 触发 FreeRTOS 的上下文切换机制（若有更高优先级任务因中断解锁） */
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

/**
 * @brief 在中断上下文中调用的收尾函数
 * @details 
 * 本函数是 `UltrasonicPwm_HandleIrq` 的核心下游。
 * 其操作必须极其迅速：立即关闭 PWM 输出、关闭更新中断，并**立刻解除接收保护通路 (`ULTRASONIC_RX_CMP_ON`)**。
 * 这样做能保证超声波探头在停止震荡的瞬间，接收电路就处于全开状态，以便捕捉极近距离的回波。
 */
static void UltrasonicPwm_FinishFromISR(UltrasonicPwm *self)
{
    /* 立即停止硬件输出 */
    HAL_TIM_PWM_Stop(self->handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(self->handle, TIM_CHANNEL_2);
    __HAL_TIM_DISABLE_IT(self->handle, TIM_IT_UPDATE);

    /* 状态回转 */
    self->burst_pending = 0U;
    self->active = 0U;
    self->cycle_count = 0U;

    /* 极其关键的切换：关闭发射，进入回波监听状态 */
    ULTRASONIC_TX_CMP_OFF();
    ULTRASONIC_RX_CMP_ON();
}

/**
 * @brief 初始化超声波底层的 GPIO 引脚复用与控制线
 * @details 
 * - 配置 CH1 / CH2 的复用引脚 (Alternate Function Mode)。
 * - 配置收发控制逻辑门引脚 (TX_CTRL / RX_CTRL) 为推挽输出状态。
 */
static void UltrasonicPwm_GpioInit(void)
{
    GPIO_InitTypeDef gpio_init;

    /* 开启硬件对应的 GPIO 端口时钟 */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* 1. 配置 PWM 差分驱动输出引脚 */
    gpio_init.Pin = ULTRASONIC_PWM_CH1_PIN | ULTRASONIC_PWM_CH2_PIN;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Alternate = ULTRASONIC_PWM_GPIO_AF;
    HAL_GPIO_Init(ULTRASONIC_PWM_GPIO_PORT, &gpio_init);

    /* 2. 配置收发控制门的切相引脚 (控制外部的模拟开关或硬件比较器) */
    gpio_init.Pin = ULTRASONIC_RX_CMP_CTRL_PIN | ULTRASONIC_TX_CMP_CTRL_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &gpio_init);
}

/* =========================================================================
 * 暴露给外部底层硬件中断的公共包装器
 * ========================================================================= */

/**
 * @brief 绑定至启动文件 (startup_stm32f4xx.s) 中的硬件 TIM4 中断向量
 */
void ULTRASONIC_PWM_IRQHandler(void)
{
    g_ultrasonic_pwm.ops->handle_irq(&g_ultrasonic_pwm);
}
