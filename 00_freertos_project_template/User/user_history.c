/**
 * @file User/user_history.c
 * @brief 测量结果历史记录服务。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "user_history.h"
#include <string.h>

/* =========================================================================
 * 全局与静态变量
 * ========================================================================= */
/** @brief 全局唯一的历史记录管理实例 */
UserResultHistory g_user_history;

/* =========================================================================
 * 内部私有方法前置声明
 * ========================================================================= */
static void UserResultHistory_Init(UserResultHistory *self);
static void UserResultHistory_Push(UserResultHistory *self, const UserMeasureResult *result);
static uint8_t UserResultHistory_Count(const UserResultHistory *self);

/* =========================================================================
 * 虚函数表定义 (V-Table)
 * ========================================================================= */
static const UserResultHistoryOps k_user_history_ops =
{
    UserResultHistory_Init,
    UserResultHistory_Push,
    UserResultHistory_Count
};

/**
 * @brief 历史记录对象构造函数
 * @param self 指向待构造的历史记录对象实例
 */
void UserResultHistory_Construct(UserResultHistory *self)
{
    /* 清空结构体，赋初值 */
    memset(self, 0, sizeof(*self));
    /* 挂载虚函数表接口 */
    self->ops = &k_user_history_ops;
}

/**
 * @brief 初始化历史记录环境
 * @details
 * 使用 FreeRTOS 的 pvPortMalloc 申请链表控制块和节点内存池。
 * 仅在系统启动初始化阶段调用一次，不会在运行中造成内存碎片。
 */
static void UserResultHistory_Init(UserResultHistory *self)
{
    uint8_t index;

    /* 防御性编程：防止被多次重复初始化 */
    if (self->ready != 0U)
    {
        return;
    }

    /* 动态分配内存池：分配链表根节点和数据节点数组 */
    self->list = (List_t *)pvPortMalloc(sizeof(List_t));
    self->nodes = (UserResultHistoryNode *)pvPortMalloc(sizeof(UserResultHistoryNode) *
                                                        USER_RESULT_HISTORY_LEN);

    /* 内存分配失败的异常处理（防止野指针） */
    if ((self->list == NULL) || (self->nodes == NULL))
    {
        if (self->list != NULL)
        {
            vPortFree(self->list);
            self->list = NULL;
        }
        if (self->nodes != NULL)
        {
            vPortFree(self->nodes);
            self->nodes = NULL;
        }
        return;
    }

    /* 初始化 FreeRTOS 链表控制块 */
    vListInitialise(self->list);
    
    /* 初始化内存池中的每一个节点 */
    for (index = 0U; index < USER_RESULT_HISTORY_LEN; index++)
    {
        memset(&self->nodes[index], 0, sizeof(self->nodes[index]));
        vListInitialiseItem(&self->nodes[index].list_item);
        /* 将节点的所有者 (Owner) 绑定为数据载荷 result 的地址，方便后续通过节点逆向查找数据 */
        listSET_LIST_ITEM_OWNER(&self->nodes[index].list_item, &self->nodes[index].result);
    }

    self->write_index = 0U;
    self->ready = 1U;
}

/**
 * @brief 将最新的测量结果压入历史记录缓存 (FIFO / 环形覆盖逻辑)
 * @param self 指向历史记录对象实例
 * @param result 指向外部传入的最新测量结果
 */
static void UserResultHistory_Push(UserResultHistory *self, const UserMeasureResult *result)
{
    ListItem_t *item;
    UserResultHistoryNode *node;

    if (self->ready == 0U)
    {
        return;
    }

    /* 进入临界区：保护全局链表操作，防止被其他任务或中断打断 */
    taskENTER_CRITICAL();
    
    /* 1. 通过环形游标取出当前可用的物理内存节点 */
    node = &self->nodes[self->write_index];
    item = &node->list_item;

    /* 2. 如果该物理节点当前已经挂载在链表中（即缓存已满，触发覆盖），先将其从原位置移除 */
    if (listLIST_ITEM_CONTAINER(item) != NULL)
    {
        (void)uxListRemove(item);
    }

    /* 3. 更新该物理节点的数据载荷 */
    node->result = *result;
    listSET_LIST_ITEM_OWNER(item, &node->result);
    
    /* 使用系统时钟 Tick 作为排序权重值 (Value)，赋予它时间属性 */
    listSET_LIST_ITEM_VALUE(item, xTaskGetTickCount());
    
    /* 4. 将更新后的节点重新插回链表尾部（保持时间先后顺序） */
    vListInsertEnd(self->list, item);

    /* 5. 游标移动与环形回滚 */
    self->write_index++;
    if (self->write_index >= USER_RESULT_HISTORY_LEN)
    {
        self->write_index = 0U;
    }
    
    /* 退出临界区 */
    taskEXIT_CRITICAL();
}

/**
 * @brief 获取当前缓存中的有效历史记录条数
 * @return 0 ~ USER_RESULT_HISTORY_LEN
 */
static uint8_t UserResultHistory_Count(const UserResultHistory *self)
{
    UBaseType_t count;

    if (self->ready == 0U)
    {
        return 0U;
    }

    /* 读取链表长度也需要进出临界区，防止读取瞬间发生链表节点的增删 */
    taskENTER_CRITICAL();
    count = listCURRENT_LIST_LENGTH(self->list);
    taskEXIT_CRITICAL();

    /* 安全性裁剪：强制将返回值限制在系统设定最大容量以内 */
    if (count > USER_RESULT_HISTORY_LEN)
    {
        count = USER_RESULT_HISTORY_LEN;
    }
    return (uint8_t)count;
}
