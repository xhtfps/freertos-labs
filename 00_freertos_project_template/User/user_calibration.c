/**
 * @file User/user_calibration.c
 * @brief 超声测距校准数据管理与时间到距离换算模块。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "user_calibration.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* 标准参考距离点：对应校准时的物理标定距离 (mm) */
static const uint16_t k_user_calib_distance_mm[USER_CALIB_POINT_COUNT] =
{
    100U, 600U, 900U, 1300U
};

/* 全局唯一的校准实例 */
UserCalibration g_user_calibration;

/* =========================================================================
 * 私有辅助函数前置声明
 * ========================================================================= */
static void UserCalibration_Init(UserCalibration *self);
static uint8_t UserCalibration_IsValidObject(const UserCalibration *self);
static uint8_t UserCalibration_Save(UserCalibration *self, const UserCalibData *calib);
static float UserCalibration_ToDistance(const UserCalibration *self, uint32_t echo_us);
static float UserCalibration_ToDefaultDistance(uint32_t echo_us);
static const UserCalibData *UserCalibration_Data(const UserCalibration *self);
static uint8_t UserCalibration_CheckData(const UserCalibData *calib);

/* =========================================================================
 * 虚函数表定义 (V-Table)
 * ========================================================================= */
static const UserCalibrationOps k_user_calibration_ops =
{
    UserCalibration_Init,
    UserCalibration_IsValidObject,
    UserCalibration_Save,
    UserCalibration_ToDistance,
    UserCalibration_ToDefaultDistance,
    UserCalibration_Data
};

/**
 * @brief 校准服务构造函数
 */
void UserCalibration_Construct(UserCalibration *self)
{
    memset(self, 0, sizeof(*self));
    self->ops = &k_user_calibration_ops;
}

/**
 * @brief 初始化：从非易失性 Flash 区域读取并校验数据
 */
static void UserCalibration_Init(UserCalibration *self)
{
    /* 强制映射 Flash 地址，读取原始结构体数据 */
    const UserCalibData *stored = (const UserCalibData *)USER_CALIB_FLASH_ADDR;

    self->data = *stored;
    /* 通过魔数、版本和数值逻辑校验数据的合法性 */
    self->valid = UserCalibration_CheckData(&self->data);
}

/**
 * @brief 获取校准数据是否有效的状态
 */
static uint8_t UserCalibration_IsValidObject(const UserCalibration *self)
{
    return self->valid;
}

/**
 * @brief 将校准数据存入 STM32 内部 Flash 扇区
 * @details 执行步骤：解锁 -> 清除状态 -> 扇区擦除 -> 字写入 -> 锁定
 */
static uint8_t UserCalibration_Save(UserCalibration *self, const UserCalibData *calib)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0U;
    uint32_t address = USER_CALIB_FLASH_ADDR;
    const uint32_t *words = (const uint32_t *)calib;
    uint32_t index;

    /* 写入前再次校验 */
    if (UserCalibration_CheckData(calib) == 0U)
    {
        return 0U;
    }

    /* Flash 操作序列 */
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_SECTOR_11;
    erase_init.NbSectors = 1U;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

    if (status == HAL_OK)
    {
        /* 以 Word 为单位写入 */
        for (index = 0U; index < (sizeof(UserCalibData) / sizeof(uint32_t)); index++)
        {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, words[index]);
            if (status != HAL_OK)
            {
                break;
            }
            address += sizeof(uint32_t);
        }
    }

    HAL_FLASH_Lock();

    /* 若 Flash 写入成功，同步更新内部缓存 */
    if (status == HAL_OK)
    {
        self->data = *calib;
        self->valid = 1U;
    }

    return (uint8_t)(status == HAL_OK);
}

/**
 * @brief 应用分段线性插值算法转换距离
 */
static float UserCalibration_ToDistance(const UserCalibration *self, uint32_t echo_us)
{
    uint8_t index;
    float x0, x1, y0, y1, distance;

    /* 降级处理：若校准失效，使用默认转换公式 */
    if (self->valid == 0U)
    {
        return UserCalibration_ToDefaultDistance(echo_us);
    }

    /* 定位 echo_us 落在哪两个校准点之间 */
    if (echo_us <= self->data.point_us[1])
    {
        index = 0U;
    }
    else if (echo_us <= self->data.point_us[2])
    {
        index = 1U;
    }
    else
    {
        index = 2U;
    }

    x0 = (float)self->data.point_us[index];
    x1 = (float)self->data.point_us[index + 1U];
    y0 = (float)k_user_calib_distance_mm[index];
    y1 = (float)k_user_calib_distance_mm[index + 1U];

    /* 防御：避免除以零 */
    if (x1 <= x0)
    {
        return UserCalibration_ToDefaultDistance(echo_us);
    }

    /* 插值计算：y = y0 + (x - x0) * (y1 - y0) / (x1 - x0) */
    distance = y0 + ((float)echo_us - x0) * (y1 - y0) / (x1 - x0);

    /* 边界硬限制（防止极端异常数据） */
    if (distance < 10.0f) distance = 10.0f;
    if (distance > 1300.0f) distance = 1300.0f;
    
    return distance;
}

/**
 * @brief 默认物理公式转换 (340m/s 声速)
 */
static float UserCalibration_ToDefaultDistance(uint32_t echo_us)
{
    /* 0.1715 是声速 343m/s 除以 2 的微秒转换因子 */
    float distance = (float)echo_us * 0.1715f;

    if (distance < 10.0f) distance = 10.0f;
    if (distance > 1300.0f) distance = 1300.0f;
    
    return distance;
}

/**
 * @brief 获取当前校准数据指针
 */
static const UserCalibData *UserCalibration_Data(const UserCalibration *self)
{
    return &self->data;
}

/**
 * @brief 校验 Flash 中的数据结构合法性
 */
static uint8_t UserCalibration_CheckData(const UserCalibData *calib)
{
    uint8_t index;

    /* 魔数与版本号校验 */
    if ((calib->magic != USER_CALIB_FLASH_MAGIC) ||
        (calib->version != USER_CALIB_FLASH_VERSION))
    {
        return 0U;
    }

    /* 数据点逻辑校验：必须单调递增，且不得超过系统量程 */
    for (index = 0U; index < USER_CALIB_POINT_COUNT; index++)
    {
        if ((calib->point_us[index] == 0U) ||
            (calib->point_us[index] > USER_ULTRASONIC_TIMEOUT_US))
        {
            return 0U;
        }

        if ((index > 0U) &&
            (calib->point_us[index] <= calib->point_us[index - 1U]))
        {
            return 0U;
        }
    }

    return 1U;
}
