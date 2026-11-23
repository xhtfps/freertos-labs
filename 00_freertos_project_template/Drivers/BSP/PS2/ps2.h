/**
 * @file Drivers/BSP/PS2/ps2.h
 * @brief PS/2 键盘采样、扫描码解析和逻辑键值状态机模块。
 * @details
 * - 属于板级外设驱动层，负责把具体硬件资源封装成清晰接口。
 * - 对外保留兼容函数，同时将状态和行为收敛到对象结构体中。
 * - 中断或 GPIO 细节不向业务层泄漏，降低模块耦合。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef __PS2_H
#define __PS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "./SYSTEM/sys/sys.h"
#include <stdbool.h>

/******************************************************************************************/
/* PS2 硬件引脚定义与抽象宏 */

#define PS2_CLK_GPIO_PORT           GPIOA
#define PS2_CLK_GPIO_PIN            GPIO_PIN_12
/** @brief 使能 CLK 引脚所在的 GPIOA 时钟 (使用 do-while(0) 结构确保宏展开时的语法安全) */
#define PS2_CLK_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define PS2_DATA_GPIO_PORT          GPIOA
#define PS2_DATA_GPIO_PIN           GPIO_PIN_11
/** @brief 使能 DATA 引脚所在的 GPIOA 时钟 */
#define PS2_DATA_GPIO_CLK_ENABLE()  do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

/**
 * @brief  读取 PS2_DATA 引脚的实时电平
 * @return uint8_t: 引脚电平状态值 (GPIO_PIN_RESET 或 GPIO_PIN_SET)
 * @note   在 EXTI 中断服务函数中被高频调用，用以在时钟下降沿锁存数据
 */
#define PS2_DATA_READ()             HAL_GPIO_ReadPin(PS2_DATA_GPIO_PORT, PS2_DATA_GPIO_PIN)

/******************************************************************************************/
/* PS/2 时序与键盘状态机时序参数定义 */

#define PS2_TIMEOUT_MS          10          /* 物理层单次位传输超时时间 (超过此时间未收到后续bit则重置接收状态机) */
#define PS2_DEBOUNCE_MS         20          /* 软件消抖时间阈值 (单位: ms，消除机械按键接触时产生的毛刺) */
#define PS2_LONG_PRESS_MS       500         /* 长按键判别阈值 (单位: ms，按住超过此时间即判定触发长按) */
#define PS2_CONTINUOUS_MS       100         /* 连击键重复触发间隔 (单位: ms，长按不松开时，键值高频重复上报的周期) */

/* PS/2 协议特殊控制扫描码 (Scan Code Set 2 标准) */
#define PS2_BREAK_CODE          0xF0        /* 断码前缀：标识随后的扫描码对应的按键被松开 (Released) */
#define PS2_EXTEND_CODE         0xE0        /* 扩展码前缀：标识当前按键属于扩展键群 (如小键盘Enter、方向键等) */

/******************************************************************************************/
/* 类型定义 (枚举与状态结构体) */

/**
 * @brief  按键触发行为模式枚举
 * @details 由底层状态机分步推导，用于判定按键生命周期所处的阶段
 */
typedef enum
{
    PS2_KEY_MODE_NONE        = 0,    /* 无按键按下/闲置状态 */
    PS2_KEY_MODE_SHORT       = 1,    /* 短按键：成功消抖并按下，尚未达到长按时间阈值 */
    PS2_KEY_MODE_LONG        = 2,    /* 长按键：按下时间已超过 PS2_LONG_PRESS_MS */
    PS2_KEY_MODE_CONTINUOUS  = 3     /* 持续按键：长按后继续保持按下，进入高频周期连击状态 */
} PS2_KeyMode_t;

/**
 * @brief  PS2 键盘扫描码映射表 (标准IBM第二套扫描码 - Scan Code Set 2)
 * @details 专针对数字小键盘 (Keypad) 及部分功能键的通码 (Make Code) 进行硬编码
 * @note   扩展键 (如小键盘 / 和 Enter) 在物理层会先发送 0xE0 前缀，此处仅定义其核心字节
 */
typedef enum
{
    PS2_SCANCODE_0          = 0x70,
    PS2_SCANCODE_1          = 0x69,
    PS2_SCANCODE_2          = 0x72,
    PS2_SCANCODE_3          = 0x7A,
    PS2_SCANCODE_4          = 0x6B,
    PS2_SCANCODE_5          = 0x73,
    PS2_SCANCODE_6          = 0x74,
    PS2_SCANCODE_7          = 0x6C,
    PS2_SCANCODE_8          = 0x75,
    PS2_SCANCODE_9          = 0x7D,
    PS2_SCANCODE_ENTER      = 0x5A,   /* 小键盘 Enter 完整扫描码为 0xE0 0x5A */
    PS2_SCANCODE_ADD        = 0x79,   /* 小键盘 + */
    PS2_SCANCODE_MINUS      = 0x7B,   /* 小键盘 - */
    PS2_SCANCODE_MULTIPLY   = 0x7C,   /* 小键盘 * */
    PS2_SCANCODE_DIVIDE     = 0x4A,   /* 小键盘 / 完整扫描码为 0xE0 0x4A */
    PS2_SCANCODE_NUMLOCK    = 0x77,   /* NumLock 键 */
    PS2_SCANCODE_POINT      = 0x71,   /* 小键盘小数点 . */
    PS2_SCANCODE_BACKSPACE  = 0x66,   /* 退格键 Backspace */
    PS2_SCANCODE_EXT_PREFIX = 0xE0    /* 协议层的扩展键标识前缀 */
} PS2_ScanCode_t;

/**
 * @brief  规范化的逻辑键值枚举 (面向应用业务层)
 * @details 解耦底层硬件扫描码，应用层开发人员无需感知 PS/2 的 0x70、0x69 等复杂码值
 */
typedef enum
{
    PS2_KEY_VALUE_0         = 0,
    PS2_KEY_VALUE_1         = 1,
    PS2_KEY_VALUE_2         = 2,
    PS2_KEY_VALUE_3         = 3,
    PS2_KEY_VALUE_4         = 4,
    PS2_KEY_VALUE_5         = 5,
    PS2_KEY_VALUE_6         = 6,
    PS2_KEY_VALUE_7         = 7,
    PS2_KEY_VALUE_8         = 8,
    PS2_KEY_VALUE_9         = 9,
    PS2_KEY_VALUE_POINT     = 10,     /* 小数点键值 */
    PS2_KEY_VALUE_BACKSPACE = 11,     /* 退格键键值 */
    PS2_KEY_VALUE_ENTER     = 12,     /* 回车键键值 */
    PS2_KEY_VALUE_ADD       = 13,     /* 加号键值 */
    PS2_KEY_VALUE_MINUS     = 14,     /* 减号键值 */
    PS2_KEY_VALUE_MULTIPLY  = 15,     /* 乘号键值 */
    PS2_KEY_VALUE_DIVIDE    = 16,     /* 除号键值 */
    PS2_KEY_VALUE_NUMLOCK   = 17,     /* 数字锁定键值 */
    PS2_KEY_VALUE_NULL      = 20      /* 闲置/无有效按键触发状态 */
} PS2_KeyValue_t;

/**
 * @brief  PS2 键盘高级核心状态机结构体
 * @details 维系并跟踪单个按键在时间轴上的变化 (消抖、按下、保持、长按、松开)
 * @note   所有成员均被 volatile 修饰，防止编译器对任务层和外部中断间的交叉共享变量进行越权优化
 */
typedef struct
{
    volatile uint16_t state_flag;       /* 状态机内部私有标志位 (保留扩展用) */
    volatile uint8_t  current_scancode; /* 正在处理的实时通码 (按下时锁存，直到收到该键的断码才释放) */
    volatile uint8_t  last_scancode;    /* 经历过消抖确立后的最新有效扫描码 */
    volatile PS2_KeyMode_t key_mode;    /* 当前按键所处的时间触发行为模式 */
    volatile uint16_t key_counter;      /* 状态机内部计时器：承载长按/连击时间步进 */
    volatile uint16_t filter_counter;   /* 状态机内部计时器：承载软件消抖倒计时 */
} PS2_KeyStateMachine_t;

typedef struct PS2_Keyboard PS2_Keyboard;

/**
 * @brief PS/2 键盘对象操作表。
 * @details 将物理层位接收、扫描码处理、按键状态机推进和应用层取键分离。
 */
typedef struct
{
    void (*init)(PS2_Keyboard *self);                                      /**< 初始化 GPIO/EXTI 和键盘状态。 */
    bool (*read_raw)(PS2_Keyboard *self, uint8_t *scan_code);              /**< 读取一字节完整扫描码。 */
    void (*sweep)(PS2_Keyboard *self, PS2_KeyStateMachine_t *fsm);         /**< 周期推进消抖、长按和连击状态。 */
    void (*scan)(PS2_Keyboard *self);                                      /**< 消费原始扫描码并更新逻辑键值。 */
    PS2_KeyValue_t (*get_key)(PS2_Keyboard *self);                         /**< 读取当前应用层逻辑键值。 */
    void (*clear)(PS2_Keyboard *self);                                     /**< 清空扫描状态和逻辑键值。 */
    void (*handle_irq)(PS2_Keyboard *self);                                /**< 在 EXTI 中断中采样 PS/2 数据位。 */
} PS2_KeyboardOps;

/**
 * @brief PS/2 键盘对象。
 * @details
 * 中断上下文负责逐位填充 rx_buffer，任务上下文通过 scan/sweep 把扫描码转换为稳定键值。
 * volatile 字段用于保护 ISR 与任务共享状态，避免编译器错误缓存。
 */
struct PS2_Keyboard
{
    PS2_KeyStateMachine_t *fsm;             /**< 上层按键状态机对象。 */
    volatile PS2_KeyValue_t *current_key;   /**< 应用层读取的最终逻辑键值。 */
    volatile uint8_t rx_buffer;             /**< ISR 按位移入的一帧扫描码缓冲。 */
    volatile uint8_t rx_bit_count;          /**< 当前帧已经接收的 bit 数。 */
    volatile bool rx_complete;              /**< 一帧扫描码接收完成标志。 */
    volatile bool is_break_code;            /**< 已接收到断码前缀 0xF0，下一码表示按键释放。 */
    volatile bool is_extend_code;           /**< 已接收到扩展码前缀 0xE0。 */
    volatile uint16_t rx_timeout;           /**< 接收超时计数，用于丢弃半帧异常数据。 */
    const PS2_KeyboardOps *ops;             /**< 键盘对象方法表。 */
};

/******************************************************************************************/
/* 导出全局变量声明 (供其他应用/驱动模块访问) */

extern PS2_KeyStateMachine_t g_ps2KeyStateMachine;  /* 全局键盘高级状态机实例 */
extern volatile PS2_KeyValue_t g_ps2CurrentKeyValue;/* 最终输出的逻辑键值 (应用层读取源) */
extern PS2_Keyboard g_ps2_keyboard;                  /* 面向对象封装后的默认键盘实例 */

/******************************************************************************************/
/* 按键状态条件检索与快速判定查询宏 */

/** * @brief 核心条件判定宏：判定目标扫描码是否达成了特定的按键模式 
 * @param fsm      指向键盘状态机结构体的指针
 * @param scancode 待判定的目标扫描码 (PS2_ScanCode_t)
 */
#define PS2_KEY_IS_SHORT_PRESSED(fsm, scancode) \
    (((fsm)->last_scancode == (scancode)) && ((fsm)->key_mode == PS2_KEY_MODE_SHORT))

#define PS2_KEY_IS_LONG_PRESSED(fsm, scancode) \
    (((fsm)->last_scancode == (scancode)) && ((fsm)->key_mode == PS2_KEY_MODE_LONG))

#define PS2_KEY_IS_CONTINUOUS(fsm, scancode) \
    (((fsm)->last_scancode == (scancode)) && ((fsm)->key_mode == PS2_KEY_MODE_CONTINUOUS))

/* ---------------------------------------------------------------------------------------- */
/* 常用按键短按模式的快捷一键查询宏 (主要供 PS2_KeyScan 内部或外部上层业务快速复核使用) */
#define PS2_KEY_0_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_0)
#define PS2_KEY_1_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_1)
#define PS2_KEY_2_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_2)
#define PS2_KEY_3_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_3)
#define PS2_KEY_4_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_4)
#define PS2_KEY_5_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_5)
#define PS2_KEY_6_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_6)
#define PS2_KEY_7_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_7)
#define PS2_KEY_8_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_8)
#define PS2_KEY_9_SHORT()           PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_9)
#define PS2_KEY_ENTER_SHORT()       PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_ENTER)
#define PS2_KEY_BACKSPACE_SHORT()   PS2_KEY_IS_SHORT_PRESSED(&g_ps2KeyStateMachine, PS2_SCANCODE_BACKSPACE)

/******************************************************************************************/
/* 导出函数声明 (API 接口定义) */

/**
 * @brief  PS2键盘外设底层硬件及软件状态机初始化
 * @note   必须在系统初始化阶段 (OS启动前或启动初期) 调用且仅调用一次
 */
void PS2_Keyboard_Init(void);

/**
 * @brief  从中断底层的单字节接收缓冲区中提取原始扫描码
 * @param  pScanCode [out] 用于接收原始扫描码字节的指针
 * @return bool: true 代表成功提取到了一个新扫描码，false 代表当前无新数据
 */
bool PS2_ReadRawScanCode(uint8_t* pScanCode);

/**
 * @brief  键盘状态机时间轴心跳步进推进函数
 * @param  pKeyFSM 指向需要维护的全局状态机实例指针
 * @note   [关键] 必须由 FreeRTOS 的定时处理任务 (如 Key_Task) 按固定周期 (如 1ms 或 10ms) 轮询调用
 */
void PS2_KeyStateMachine_Sweep(PS2_KeyStateMachine_t* pKeyFSM);

/**
 * @brief  高级键值翻译与条件消费引擎
 * @details 负责检测宏条件、映射并更新全局 `g_ps2CurrentKeyValue`，同时复位已被消费的状态机分支
 * @note   建议在主循环或高优先级按键处理任务中周期性轮询调用
 */
void PS2_KeyScan(void);

/**
 * @brief  供上层应用业务获取当前被触发的逻辑键值 (应用层核心API)
 * @return PS2_KeyValue_t: 转换后的标准逻辑键值。若无有效按键则返回 `PS2_KEY_VALUE_NULL`
 * @note   该函数具备“读后即清”的消费特征，保证单次按键仅被业务逻辑处理一次
 */
PS2_KeyValue_t PS2_GetCurrentKeyValue(void) ;

/**
 * @brief  强制重置底层物理层接收参数和上层按键状态机
 * @details 适用于底层发生物理超时或应用切换、系统发生软硬件异常时的系统状态对齐
 */
void PS2_ClearStateMachine(void);

#ifdef __cplusplus
}
#endif

#endif /* __PS2_H */
