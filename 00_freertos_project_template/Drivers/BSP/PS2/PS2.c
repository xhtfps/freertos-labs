/**
 * @file Drivers/BSP/PS2/ps2.c
 * @brief PS/2 键盘采样、扫描码解析和逻辑键值状态机模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "./BSP/PS2/ps2.h"

/* PS/2 接收一帧数据（11 bits）的最大超时时间。用于防止时钟线干扰导致的接收死锁 */
#define PS2_RX_TIMEOUT_MS      10U

/* =========================================================================
 * 全局与静态变量
 * ========================================================================= */
/** @brief PS/2 键盘按键状态机全局实例 */
PS2_KeyStateMachine_t g_ps2KeyStateMachine = {0};

/** @brief 当前解析出的有效键值缓存 (使用 volatile 防止编译器在中断/轮询并发时过度优化) */
volatile PS2_KeyValue_t g_ps2CurrentKeyValue = PS2_KEY_VALUE_NULL;

/* =========================================================================
 * 内部私有方法前置声明
 * ========================================================================= */
static void PS2_KeyboardObject_Init(PS2_Keyboard *self);
static bool PS2_KeyboardObject_ReadRaw(PS2_Keyboard *self, uint8_t *scan_code);
static void PS2_KeyboardObject_Sweep(PS2_Keyboard *self, PS2_KeyStateMachine_t *fsm);
static void PS2_KeyboardObject_Scan(PS2_Keyboard *self);
static PS2_KeyValue_t PS2_KeyboardObject_GetKey(PS2_Keyboard *self);
static void PS2_KeyboardObject_Clear(PS2_Keyboard *self);
static void PS2_KeyboardObject_HandleIrq(PS2_Keyboard *self);
static void PS2_KeyboardObject_GpioInit(void);
static void PS2_KeyboardObject_ProcessScanCode(PS2_Keyboard *self, uint8_t scancode);
static PS2_KeyValue_t PS2_KeyboardObject_Translate(PS2_KeyStateMachine_t *fsm);

/* =========================================================================
 * 虚函数表定义 (V-Table)
 * ========================================================================= */
static const PS2_KeyboardOps s_ps2_keyboard_ops =
{
    PS2_KeyboardObject_Init,
    PS2_KeyboardObject_ReadRaw,
    PS2_KeyboardObject_Sweep,
    PS2_KeyboardObject_Scan,
    PS2_KeyboardObject_GetKey,
    PS2_KeyboardObject_Clear,
    PS2_KeyboardObject_HandleIrq
};

/** @brief 全局唯一的 PS/2 键盘对象实例 */
PS2_Keyboard g_ps2_keyboard =
{
    &g_ps2KeyStateMachine,
    &g_ps2CurrentKeyValue,
    0U,             // rx_buffer
    0U,             // rx_bit_count
    false,          // rx_complete
    false,          // is_break_code
    false,          // is_extend_code
    0U,             // rx_timeout
    &s_ps2_keyboard_ops
};

/* =========================================================================
 * 对象私有方法实现
 * ========================================================================= */

/**
 * @brief 初始化键盘对象及其硬件依赖
 */
static void PS2_KeyboardObject_Init(PS2_Keyboard *self)
{
    if (self == NULL)
    {
        return;
    }

    PS2_KeyboardObject_GpioInit();
    self->ops->clear(self);
}

/**
 * @brief 读取底层的原始物理扫描码
 * @param scan_code 指向用于保存扫描码的变量
 * @return true: 成功读取到新数据; false: 当前无新数据
 */
static bool PS2_KeyboardObject_ReadRaw(PS2_Keyboard *self, uint8_t *scan_code)
{
    if ((self == NULL) || (scan_code == NULL))
    {
        return false;
    }

    /* 检查中断是否已经拼接完一个完整的 8bit 字节 */
    if (self->rx_complete)
    {
        *scan_code = self->rx_buffer;
        self->rx_complete = false; /* 消费数据，清除标志 */
        return true;
    }

    return false;
}

/**
 * @brief 键盘状态机的心跳/扫描轮询函数
 * @details 
 * 需要在定时器或主循环中周期性调用（通常 1ms）。
 * 负责超时错误恢复、读取原始数据流、推进长按与连发状态机。
 */
static void PS2_KeyboardObject_Sweep(PS2_Keyboard *self, PS2_KeyStateMachine_t *fsm)
{
    uint8_t scancode;

    if ((self == NULL) || (fsm == NULL))
    {
        return;
    }

    /* 1. 接收超时恢复机制：如果接收了一半被打断，超时后重置硬件接收状态 */
    if ((self->rx_bit_count > 0U) && (!self->rx_complete))
    {
        if (++self->rx_timeout >= PS2_RX_TIMEOUT_MS)
        {
            self->rx_bit_count = 0U;
            self->rx_buffer = 0U;
            self->is_break_code = false;
            self->is_extend_code = false;
            self->rx_timeout = 0U;
        }
    }
    else
    {
        self->rx_timeout = 0U;
    }

    /* 2. 读取原始扫描码并推进解析状态机（处理断码与扩展码） */
    if (self->ops->read_raw(self, &scancode))
    {
        PS2_KeyboardObject_ProcessScanCode(self, scancode);
    }

    /* 3. 按键消抖与按压确认（滤波延迟） */
    if (fsm->filter_counter > 0U)
    {
        fsm->filter_counter--;
        if (fsm->filter_counter == 0U)
        {
            /* 消抖结束，确认当前扫描码有效 */
            fsm->last_scancode = fsm->current_scancode;
        }
    }

    /* 4. 按键长按与连发状态机推进 */
    if (fsm->key_mode != PS2_KEY_MODE_NONE)
    {
        fsm->key_counter++;

        switch (fsm->key_mode)
        {
            case PS2_KEY_MODE_SHORT:
                /* 持续按下达到长按阈值，切换至长按模式 */
                if (fsm->key_counter >= PS2_LONG_PRESS_MS)
                {
                    fsm->key_mode = PS2_KEY_MODE_LONG;
                    fsm->key_counter = 0U;
                }
                break;

            case PS2_KEY_MODE_LONG:
                /* 在长按模式下持续按下，切换至连发模式 */
                if (fsm->key_counter >= PS2_CONTINUOUS_MS)
                {
                    fsm->key_mode = PS2_KEY_MODE_CONTINUOUS;
                    fsm->key_counter = 0U;
                }
                break;

            case PS2_KEY_MODE_CONTINUOUS:
                /* 连发模式周期触发 */
                if (fsm->key_counter >= PS2_CONTINUOUS_MS)
                {
                    fsm->key_counter = 0U;
                }
                break;

            default:
                break;
        }
    }
}

/**
 * @brief 翻译状态机并提取逻辑键值
 * @details 将状态机层面的 scancode 和 press_mode 结合，翻译为顶层可用的业务键值
 */
static void PS2_KeyboardObject_Scan(PS2_Keyboard *self)
{
    PS2_KeyValue_t key_value;

    if ((self == NULL) || (self->fsm == NULL) || (self->current_key == NULL))
    {
        return;
    }

    key_value = PS2_KeyboardObject_Translate(self->fsm);
    
    if (key_value != PS2_KEY_VALUE_NULL)
    {
        /* 更新全局有效键值 */
        *(self->current_key) = key_value;
        
        /* 消费掉本次按键事件，防止重复触发 */
        self->fsm->key_mode = PS2_KEY_MODE_NONE;
        self->fsm->last_scancode = 0U;
    }
}

/**
 * @brief 业务层获取当前按键值 (带清空属性)
 * @return PS2_KEY_VALUE_NULL 表示无按键，其它枚举值表示有效按键
 */
static PS2_KeyValue_t PS2_KeyboardObject_GetKey(PS2_Keyboard *self)
{
    PS2_KeyValue_t key_value;

    if ((self == NULL) || (self->current_key == NULL))
    {
        return PS2_KEY_VALUE_NULL;
    }

    /* 读出后即清除（Read-and-Clear），标准的事件消费机制 */
    key_value = *(self->current_key);
    *(self->current_key) = PS2_KEY_VALUE_NULL;
    return key_value;
}

/**
 * @brief 清除并复位整个键盘对象的状态机和缓存
 */
static void PS2_KeyboardObject_Clear(PS2_Keyboard *self)
{
    if ((self == NULL) || (self->fsm == NULL) || (self->current_key == NULL))
    {
        return;
    }

    self->fsm->state_flag = 0U;
    self->fsm->current_scancode = 0U;
    self->fsm->last_scancode = 0U;
    self->fsm->key_mode = PS2_KEY_MODE_NONE;
    self->fsm->key_counter = 0U;
    self->fsm->filter_counter = 0U;

    self->rx_buffer = 0U;
    self->rx_bit_count = 0U;
    self->rx_complete = false;
    self->is_break_code = false;
    self->is_extend_code = false;
    self->rx_timeout = 0U;

    *(self->current_key) = PS2_KEY_VALUE_NULL;
}

/**
 * @brief PS/2 时钟下降沿中断处理函数 (解析 11-bit 数据帧)
 * @details 
 * PS/2 协议帧结构 (设备到主机): 
 * 1 bit Start(0) | 8 bits Data (LSB first) | 1 bit Parity | 1 bit Stop(1)
 */
static void PS2_KeyboardObject_HandleIrq(PS2_Keyboard *self)
{
    uint8_t sda;

    if (self == NULL)
    {
        return;
    }

    /* 校验确实是 PS2 CLK 引脚触发的中断 */
    if (__HAL_GPIO_EXTI_GET_IT(PS2_CLK_GPIO_PIN) == RESET)
    {
        return;
    }

    __HAL_GPIO_EXTI_CLEAR_IT(PS2_CLK_GPIO_PIN);
    
    /* 采样 DATA 线电平 (PS/2 必须在 CLK 下降沿采样) */
    sda = (PS2_DATA_READ() == GPIO_PIN_RESET) ? 0U : 1U;
    
    /* 接收到时钟沿，重置超时计数器 */
    self->rx_timeout = 0U;

    /* 第 0 位：起始位 (必须为 0) */
    if (self->rx_bit_count == 0U)
    {
        if (sda == 0U)
        {
            self->rx_bit_count++;
            self->rx_buffer = 0U;
        }
        return;
    }

    /* 第 1~8 位：数据位 (低位在前，所以向右移位) */
    if (self->rx_bit_count <= 8U)
    {
        self->rx_buffer >>= 1;
        if (sda != 0U)
        {
            self->rx_buffer |= 0x80U;
        }
        self->rx_bit_count++;
        return;
    }

    /* 第 9 位：奇偶校验位 (此驱动简化处理，直接跳过不校验) */
    if (self->rx_bit_count == 9U)
    {
        self->rx_bit_count++;
        return;
    }

    /* 第 10 位：停止位 (必须为 1) */
    if (self->rx_bit_count == 10U)
    {
        if (sda != 0U)
        {
            self->rx_complete = true; /* 成功接收完 1 个 Byte */
        }

        self->rx_bit_count = 0U; /* 准备接收下一帧 */
        return;
    }

    /* 异常保护：计数越界直接清零 */
    self->rx_bit_count = 0U;
}

/**
 * @brief 底层 GPIO 与 EXTI 初始化
 */
static void PS2_KeyboardObject_GpioInit(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    PS2_CLK_GPIO_CLK_ENABLE();
    PS2_DATA_GPIO_CLK_ENABLE();

    /* 初始化 CLK 引脚为下降沿触发外部中断 (用于捕获数据) */
    gpio_init_struct.Pin = PS2_CLK_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_IT_FALLING;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PS2_CLK_GPIO_PORT, &gpio_init_struct);

    /* 初始化 DATA 引脚为普通上拉输入 (用于读取数据) */
    gpio_init_struct.Pin = PS2_DATA_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(PS2_DATA_GPIO_PORT, &gpio_init_struct);

    /* 配置并使能对应的外部中断优先级 */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
 * @brief 处理单字节扫描码，解析断码和扩展码，更新状态机
 * @param scancode 硬件中断接收到的原始字节
 */
static void PS2_KeyboardObject_ProcessScanCode(PS2_Keyboard *self, uint8_t scancode)
{
    PS2_KeyStateMachine_t *fsm;

    if ((self == NULL) || (self->fsm == NULL))
    {
        return;
    }

    fsm = self->fsm;

    /* 捕获断码前缀 (0xF0)，表示按键释放 */
    if (scancode == PS2_BREAK_CODE)
    {
        self->is_break_code = true;
        return;
    }

    /* 捕获扩展码前缀 (0xE0)，多用于功能键、右侧 Shift/Ctrl、方向键等 */
    if (scancode == PS2_EXTEND_CODE)
    {
        self->is_extend_code = true;
        return;
    }

    /* 若上一字节是断码，说明当前这个字节是被释放的按键 */
    if (self->is_break_code)
    {
        if (scancode == fsm->current_scancode)
        {
            /* 按键已被松开，重置按压状态 */
            fsm->key_mode = PS2_KEY_MODE_NONE;
            fsm->key_counter = 0U;
            fsm->last_scancode = 0U;
            fsm->current_scancode = 0U;
        }

        /* 处理完毕，清除断码标志 */
        self->is_break_code = false;
        self->is_extend_code = false;
        return;
    }

    /* 如果是一个新的有效通码 (按键被按下)，开启消抖并标记为短按模式 */
    if (scancode != fsm->current_scancode)
    {
        fsm->current_scancode = scancode;
        fsm->filter_counter = PS2_DEBOUNCE_MS; /* 开始消抖滤波倒计时 */
        fsm->key_mode = PS2_KEY_MODE_SHORT;
        fsm->key_counter = 0U;
    }

    /* 扩展按键处理完后清除标志 */
    self->is_extend_code = false;
}

/**
 * @brief 将底层扫描码通过宏映射成逻辑业务键值
 * @return 翻译后的枚举键值
 */
static PS2_KeyValue_t PS2_KeyboardObject_Translate(PS2_KeyStateMachine_t *fsm)
{
    /* 使用宏 PS2_KEY_IS_SHORT_PRESSED 匹配扫描码与有效状态 */
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_0)) return PS2_KEY_VALUE_0;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_1)) return PS2_KEY_VALUE_1;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_2)) return PS2_KEY_VALUE_2;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_3)) return PS2_KEY_VALUE_3;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_4)) return PS2_KEY_VALUE_4;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_5)) return PS2_KEY_VALUE_5;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_6)) return PS2_KEY_VALUE_6;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_7)) return PS2_KEY_VALUE_7;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_8)) return PS2_KEY_VALUE_8;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_9)) return PS2_KEY_VALUE_9;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_POINT)) return PS2_KEY_VALUE_POINT;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_BACKSPACE)) return PS2_KEY_VALUE_BACKSPACE;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_ENTER)) return PS2_KEY_VALUE_ENTER;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_ADD)) return PS2_KEY_VALUE_ADD;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_MINUS)) return PS2_KEY_VALUE_MINUS;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_MULTIPLY)) return PS2_KEY_VALUE_MULTIPLY;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_DIVIDE)) return PS2_KEY_VALUE_DIVIDE;
    if (PS2_KEY_IS_SHORT_PRESSED(fsm, PS2_SCANCODE_NUMLOCK)) return PS2_KEY_VALUE_NUMLOCK;

    return PS2_KEY_VALUE_NULL;
}

/* =========================================================================
 * 暴露给外部业务层的公共 API (包装器)
 * ========================================================================= */

void PS2_Keyboard_Init(void)
{
    g_ps2_keyboard.ops->init(&g_ps2_keyboard);
}

bool PS2_ReadRawScanCode(uint8_t *pScanCode)
{
    return g_ps2_keyboard.ops->read_raw(&g_ps2_keyboard, pScanCode);
}

void PS2_KeyStateMachine_Sweep(PS2_KeyStateMachine_t *pKeyFSM)
{
    g_ps2_keyboard.ops->sweep(&g_ps2_keyboard, pKeyFSM);
}

void PS2_KeyScan(void)
{
    g_ps2_keyboard.ops->scan(&g_ps2_keyboard);
}

PS2_KeyValue_t PS2_GetCurrentKeyValue(void)
{
    return g_ps2_keyboard.ops->get_key(&g_ps2_keyboard);
}

void PS2_ClearStateMachine(void)
{
    g_ps2_keyboard.ops->clear(&g_ps2_keyboard);
}

/**
 * @brief 将硬件中断路由至对象的中断处理实体
 * @note 需确保 HAL_NVIC_EnableIRQ 使能了正确的引脚线
 */
void EXTI15_10_IRQHandler(void)
{
    g_ps2_keyboard.ops->handle_irq(&g_ps2_keyboard);
}
