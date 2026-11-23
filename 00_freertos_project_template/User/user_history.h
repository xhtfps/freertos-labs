/**
 * @file User/user_history.h
 * @brief 测量结果历史记录服务。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef USER_HISTORY_H
#define USER_HISTORY_H

#include "user_internal.h"

/* * 补充 FreeRTOS 链表类型和测量结果类型的前置声明
 * (如果 user_internal.h 中已经包含，可忽略)
 */
// #include "FreeRTOS.h"
// #include "list.h"
// typedef struct UserMeasureResult UserMeasureResult; 

#ifdef __cplusplus
extern "C" {
#endif

/* 前置声明，用于虚函数表内部引用 */
typedef struct UserResultHistory UserResultHistory;

/**
 * @brief 历史记录节点结构体（侵入式链表节点）
 * @details 
 * 将 FreeRTOS 的 ListItem_t 嵌入到用户数据结构中，
 * 这是标准的“侵入式链表”设计，无需为链表节点额外动态分配内存。
 */
typedef struct
{
    ListItem_t list_item;         /**< FreeRTOS 链表节点句柄，必须放在结构体首位或通过宏提取 */
    UserMeasureResult result;     /**< 实际的测量结果数据有效载荷 */
} UserResultHistoryNode;

/**
 * @brief 历史记录模块的操作接口 (虚函数表)
 */
typedef struct UserResultHistoryOps
{
    /** @brief 初始化历史记录链表与内部内存池 */
    void (*init)(UserResultHistory *self);
    
    /** * @brief 压入一条最新的测量结果 
     * @param result 指向最新测量数据的指针
     */
    void (*push)(UserResultHistory *self, const UserMeasureResult *result);
    
    /** * @brief 获取当前已存储的历史记录条数 
     * @return 当前链表中的有效节点数量
     */
    uint8_t (*count)(const UserResultHistory *self);
} UserResultHistoryOps;

/**
 * @brief 历史记录服务类实体结构体
 * @details
 * 结合了链表的动态特性与数组的静态内存分配优势。
 */
struct UserResultHistory
{
    const UserResultHistoryOps *ops; // 虚函数表指针

    /* === 链表与内存池状态 === */
    List_t *list;                    // 指向 FreeRTOS 链表头控制块的指针
    UserResultHistoryNode *nodes;    // 静态分配的节点数组（内存池），由外部或初始化时绑定
    uint8_t write_index;             // 环形缓冲区写入游标，指示下一个可用的 node 内存块
    
    /* === 模块状态 === */
    uint8_t ready;                   // 模块是否已成功初始化的标志位
};

/** @brief 声明全局唯一的历史记录实例 */
extern UserResultHistory g_user_history;

/**
 * @brief 历史记录模块构造函数
 * @param self 指向待构造的历史记录对象实例
 */
void UserResultHistory_Construct(UserResultHistory *self);

#ifdef __cplusplus
}
#endif

#endif /* USER_HISTORY_H */
