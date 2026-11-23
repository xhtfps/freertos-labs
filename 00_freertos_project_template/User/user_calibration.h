/**
 * @file User/user_calibration.h
 * @brief 超声测距校准数据管理与时间到距离换算模块。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef USER_CALIBRATION_H
#define USER_CALIBRATION_H

#include "user_internal.h"

/* * 补充前置声明，确保 UserCalibData 等类型在独立编译时可用。
 * 假设 UserCalibData 已在 user_internal.h 中定义。
 */
// typedef struct UserCalibData UserCalibData;

#ifdef __cplusplus
extern "C" {
#endif

/* 前置声明，用于解决结构体与函数指针互相引用的问题 */
typedef struct UserCalibration UserCalibration;

/**
 * @brief 校准服务模块的操作接口 (虚函数表)
 * @details 将具体的校准算法、存储逻辑与调用者解耦
 */
typedef struct UserCalibrationOps
{
    /** @brief 初始化校准服务（通常包括从 Flash/EEPROM 读取历史校准数据） */
    void (*init)(UserCalibration *self);
    
    /** * @brief 检查当前内存中的校准数据是否有效 
     * @return 1: 有效 (校验和正确且在合理范围内); 0: 无效或未校准
     */
    uint8_t (*is_valid)(const UserCalibration *self);
    
    /** * @brief 保存新的校准数据到非易失性存储器，并更新当前内存状态 
     * @param calib 指向待写入的新校准数据
     * @return 1: 保存成功; 0: 保存失败 (如 Flash 写入错误)
     */
    uint8_t (*save)(UserCalibration *self, const UserCalibData *calib);
    
    /** * @brief 使用当前的校准参数，将回波时间转换为精确物理距离 
     * @param echo_us 超声波往返时间 (微秒)
     * @return 补偿后的精确距离 (通常单位为 mm)
     */
    float (*to_distance)(const UserCalibration *self, uint32_t echo_us);
    
    /** * @brief 使用标准声速 (如常温 340m/s) 进行默认转换，不应用校准参数 
     * @details 作为无校准数据或校准数据损坏时的容错降级方法 (Fail-Safe)
     * @param echo_us 超声波往返时间 (微秒)
     * @return 默认的物理距离
     */
    float (*to_default_distance)(uint32_t echo_us);
    
    /** * @brief 获取当前正在使用的校准数据结构体指针 (只读)
     */
    const UserCalibData *(*data)(const UserCalibration *self);
} UserCalibrationOps;

/**
 * @brief 校准服务类实体结构体
 */
struct UserCalibration
{
    const UserCalibrationOps *ops; // 虚函数表指针
    
    /* === 模块数据状态 === */
    UserCalibData data;            // 当前生效的校准数据集（如斜率、截距、温度补偿系数等）
    uint8_t valid;                 // 标记当前 data 是否有效 (1:有效, 0:无效需使用默认值)
};

/** @brief 声明全局唯一的校准服务实例 */
extern UserCalibration g_user_calibration;

/**
 * @brief 校准模块构造函数
 * @param self 指向待构造的校准对象实例
 */
void UserCalibration_Construct(UserCalibration *self);

#ifdef __cplusplus
}
#endif

#endif /* USER_CALIBRATION_H */
