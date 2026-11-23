/**
 * @file User/user_ui.h
 * @brief LCD/PS2 交互界面和菜单状态机模块。
 * @details
 * - 负责菜单状态、LCD 显示和 PS/2 按键事件消费。
 * - 通过依赖注入持有校准、历史和测量服务，避免跨模块直接访问内部状态。
 * - 界面层只做交互编排，测距算法和硬件控制保留在服务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef USER_UI_H
#define USER_UI_H

/* 依赖的底层业务逻辑模块头文件 */
#include "user_calibration.h"
#include "user_history.h"
#include "user_measure.h"

/* * 包含 FreeRTOS 相关的头文件以识别 QueueHandle_t
 * (如果已经在其他全局基础头文件中包含，可根据项目实际情况调整)
 */
// #include "FreeRTOS.h"
// #include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前置声明：为了在 UserInterfaceOps 函数指针中合法使用 UserInterface 类型 */
typedef struct UserInterface UserInterface;

/**
 * @brief UI 模块的“虚函数表” (Operations)
 * @details 包含该对象所有对外公开的方法接口，实现了行为与数据的解耦
 */
typedef struct UserInterfaceOps
{
    /** @brief 初始化 UI 环境（如图层、队列、静态文字等） */
    void (*init)(UserInterface *self);
    
    /** @brief 运行 UI 状态机（通常在独立的 FreeRTOS 任务中死循环调用） */
    void (*run)(UserInterface *self);
    
    /** @brief 异步投递按键消息（供外部中断或按键扫描任务调用） */
    void (*post_key)(UserInterface *self, PS2_KeyValue_t key);
} UserInterfaceOps;

/**
 * @brief UI 模块的类实体结构体
 * @details 包含操作接口指针、运行时状态以及注入的底层依赖对象
 */
struct UserInterface
{
    const UserInterfaceOps *ops;   // 虚函数表指针，绑定该对象的行为接口
    
    /* === 运行时状态 === */
    QueueHandle_t key_queue;       // FreeRTOS 按键消息队列句柄，用于异步接收外部按键
    uint8_t menu;                  // 当前页面/菜单状态机 ID (0:主菜单, 1~4:子菜单)
    
    /* === 依赖注入 (Dependency Injection) === */
    UserCalibration *calibration;  // 指向全局校准模块实体的指针
    UserResultHistory *history;    // 指向全局历史记录模块实体的指针
    UserMeasureClient measure;     // 测量服务客户端对象（值拷贝，内部包含底层通信的上下文）
};

/** * @brief 声明全局唯一的 UI 实例 
 * @note 实际定义在 user_ui.c 中
 */
extern UserInterface g_user_interface;

/**
 * @brief UI 对象构造函数
 * @details 用于初始化实体的数据结构，并挂载依赖的模块实例
 * * @param self        指向待构造的 UI 对象实例
 * @param calibration 注入的校准模块依赖
 * @param history     注入的历史记录模块依赖
 * @param measure     注入的测量服务客户端依赖
 */
void UserInterface_Construct(UserInterface *self,
                             UserCalibration *calibration,
                             UserResultHistory *history,
                             const UserMeasureClient *measure);

#ifdef __cplusplus
}
#endif

#endif /* USER_UI_H */
