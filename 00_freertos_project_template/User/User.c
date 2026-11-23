/**
 * @file User/User.c
 * @brief 用户应用对象聚合、依赖注入与任务入口适配层。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "User.h"
#include "user_calibration.h"
#include "user_history.h"
#include "user_measure.h"
#include "user_ui.h"
#include "./BSP/PS2/ps2.h"

/* ========================================== *
 * 静态全局变量                  *
 * ========================================== */
#define USER_RUNTIME_UNINIT       0U
#define USER_RUNTIME_INITING      1U
#define USER_RUNTIME_READY        2U

static UserApp s_user_app;                  // 统一管理各类子模块实例的全局应用结构体
static uint8_t s_user_app_constructed;      // 构造状态标志：0-未构造，1-已构造
static uint8_t s_user_runtime_ready;        // 运行时状态：未初始化/初始化中/已完成

/* ========================================== *
 * 内部静态函数声明              *
 * ========================================== */
static void User_AppConstruct(void);
static void User_AppInit(void);
static uint8_t UserMeasureClient_RequestSingle(void *context, UserMeasureResult *result, TickType_t timeout);
static void UserMeasureClient_SendCommand(void *context, const UserMeasureCommand *cmd);
static uint8_t UserMeasureClient_ReceiveResult(void *context, UserMeasureResult *result, TickType_t timeout);

/**
 * @brief 用户应用初始化总入口
 * @note 内部包含构造与运行时初始化，供各任务调用
 */
void User_Init(void)
{
    User_AppConstruct(); // 1. 实例构造与指针挂载
    User_AppInit();      // 2. 硬件/底层状态机运行时初始化
}

/**
 * @brief 经典用户主任务（常作为独立的 FreeRTOS 任务挂载）
 */
void User_Task(void *pvParameters)
{
    (void)pvParameters;  // 防止编译器报“未使用参数”的警告
    User_main();
}

/**
 * @brief 用户主逻辑入口
 * @details 负责初始化并接管 UI 线程的无限循环
 */
void User_main(void)
{
    User_Init();
    // 调用 UI 模块虚表中的 run 函数，进入 UI 状态机主循环
    s_user_app.ui->ops->run(s_user_app.ui);
}

/**
 * @brief PWM/测量核心任务
 * @details 负责初始化并接管底层测量与信号处理（如PWM触发、数据采集）的主循环
 */
void PWM_Task(void *pvParameters)
{
    (void)pvParameters;  // 防止编译器未引用警告
    User_Init();

    // 调用测量服务模块虚表中的 run 函数，进入测量主循环
    s_user_app.measure->ops->run(s_user_app.measure);
}

/**
 * @brief 按键/键盘扫描任务（周期性 FreeRTOS 任务）
 */
void Key_Task(void *pvParameters)
{
    PS2_KeyValue_t key;
    TickType_t last_wake_time;
    uint8_t sweep;

    (void)pvParameters;
    User_Init(); // 确保应用层已初始化

    // 获取当前系统 Tick 数，用于精准周期性延时
    last_wake_time = xTaskGetTickCount();

    for (;;)
    {
        // 1. 状态机消抖与状态更新（循环执行10次以确保底层驱动状态机充分迭代）
        for (sweep = 0U; sweep < 10U; sweep++)
        {
            PS2_KeyStateMachine_Sweep(&g_ps2KeyStateMachine);
        }

        // 2. 扫描键盘并获取当前有效的键值
        PS2_KeyScan();
        key = PS2_GetCurrentKeyValue();

        // 3. 如果捕获到有效按键，通过虚表接口向 UI 模块异步邮寄(Post)键值消息
        if (key != PS2_KEY_VALUE_NULL)
        {
            s_user_app.ui->ops->post_key(s_user_app.ui, key);
        }

        // 4. 绝对延时 10ms，确保任务严格以 100Hz 频率执行
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10));
    }
}

/**
 * @brief 检查系统当前是否处于忙碌状态（通常指测量模块是否在工作）
 * @return uint8_t 1-忙碌，0-空闲/未就绪
 */
uint8_t User_IsBusy(void)
{
    // 如果运行时环境都还没准备好，直接返回不忙碌
    if (s_user_runtime_ready != USER_RUNTIME_READY)
    {
        return 0U;
    }

    // 调用测量服务虚表接口，查询当前硬件/算法状态是否忙碌
    return s_user_app.measure->ops->is_busy(s_user_app.measure);
}

/**
 * @brief 构造用户应用层各个子模块实例
 * @note 由于该函数会被各个 FreeRTOS 任务并发调用，内部使用了临界区保护以防止重复构造
 */
static void User_AppConstruct(void)
{
    UserMeasureClient measure_client;

    // 双重检查锁定（第一重）：若已构造则直接返回，避免高频进入临界区
    if (s_user_app_constructed != 0U)
    {
        return;
    }

    // 进入 FreeRTOS 临界区，关闭中断，防止多任务抢占导致并发构造冲突
    taskENTER_CRITICAL();
    {
        // 双重检查锁定（第二重）：确保在临界区内安全的进行单例构造
        if (s_user_app_constructed == 0U)
        {
            // 1. 构造底层实体模块（校准、历史记录、测量服务）
            UserCalibration_Construct(&g_user_calibration);
            UserResultHistory_Construct(&g_user_history);
            UserMeasureService_Construct(&g_user_measure_service,
                                         &g_user_calibration,
                                         &g_user_history);

            // 2. 组装测量客户端接口体（作为依赖注入到 UI 模块中）
            measure_client.context = &g_user_measure_service;
            measure_client.request_single = UserMeasureClient_RequestSingle;
            measure_client.send_command = UserMeasureClient_SendCommand;
            measure_client.receive_result = UserMeasureClient_ReceiveResult;

            // 3. 构造 UI 界面模块（将校准、历史和测量客户端注入其中）
            UserInterface_Construct(&g_user_interface,
                                    &g_user_calibration,
                                    &g_user_history,
                                    &measure_client);

            // 4. 将全局实例的指针统一挂载到 s_user_app 结构体中集中管理
            s_user_app.calibration = &g_user_calibration;
            s_user_app.history = &g_user_history;
            s_user_app.measure = &g_user_measure_service;
            s_user_app.ui = &g_user_interface;
            
            // 标记构造完成
            s_user_app_constructed = 1U;
        }
    }
    // 退出 FreeRTOS 临界区，恢复中断
    taskEXIT_CRITICAL();
}

/**
 * @brief 运行环境/驱动初始化
 * @note 串行调用各子模块虚表中的 init 函数，配置其初始运行参数或状态
 */
static void User_AppInit(void)
{
    uint8_t do_init = 0U;

    // 如果已经初始化过，直接返回
    if (s_user_runtime_ready == USER_RUNTIME_READY)
    {
        return;
    }

    for (;;)
    {
        taskENTER_CRITICAL();
        if (s_user_runtime_ready == USER_RUNTIME_READY)
        {
            taskEXIT_CRITICAL();
            return;
        }
        if (s_user_runtime_ready == USER_RUNTIME_UNINIT)
        {
            s_user_runtime_ready = USER_RUNTIME_INITING;
            do_init = 1U;
            taskEXIT_CRITICAL();
            break;
        }
        taskEXIT_CRITICAL();

        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    if (do_init == 0U)
    {
        return;
    }

    // 依次调用各个子模块的运行时初始化接口
    s_user_app.history->ops->init(s_user_app.history);
    s_user_app.calibration->ops->init(s_user_app.calibration);
    s_user_app.measure->ops->init(s_user_app.measure);
    s_user_app.ui->ops->init(s_user_app.ui);

    taskENTER_CRITICAL();
    s_user_runtime_ready = USER_RUNTIME_READY;
    taskEXIT_CRITICAL();
}

/* ========================================================================= *
 * 以下为 UserMeasureClient 接口虚表回调函数的具体实现                       *
 * （本质上是把 UI 层传来的泛型 context 指针强转回具体的测量服务对象并调用其方法） *
 * ========================================================================= */

/**
 * @brief 客户端发起单次测量请求
 */
static uint8_t UserMeasureClient_RequestSingle(void *context, UserMeasureResult *result, TickType_t timeout)
{
    UserMeasureService *measure = (UserMeasureService *)context;
    return measure->ops->request_single(measure, result, timeout);
}

/**
 * @brief 客户端发送测量命令
 */
static void UserMeasureClient_SendCommand(void *context, const UserMeasureCommand *cmd)
{
    UserMeasureService *measure = (UserMeasureService *)context;
    measure->ops->send_command(measure, cmd);
}

/**
 * @brief 客户端接收/读取测量结果
 */
static uint8_t UserMeasureClient_ReceiveResult(void *context, UserMeasureResult *result, TickType_t timeout)
{
    UserMeasureService *measure = (UserMeasureService *)context;
    return measure->ops->receive_result(measure, result, timeout);
}
