/**
 * @file User/user_internal.h
 * @brief 用户应用内部公共常量、依赖接口与对象前置声明。
 * @details
 * - 属于用户应用层，负责业务对象组合、任务入口、状态机或公共数据契约。
 * - 模块之间通过结构体指针和函数表协作，尽量避免跨模块直接访问内部状态。
 * - 该层保持业务编排职责，底层硬件细节下沉到驱动对象。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#ifndef USER_INTERNAL_H
#define USER_INTERNAL_H

#include "User.h"
#include "./BSP/PS2/ps2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USER_MEASURE_QUEUE_LEN       4U
#define USER_RESULT_QUEUE_LEN        1U
#define USER_RESULT_HISTORY_LEN      8U
#define USER_KEY_QUEUE_LEN           8U

#define USER_CALIB_POINT_COUNT       4U
#define USER_CALIB_FLASH_ADDR        0x080E0000U
#define USER_CALIB_FLASH_MAGIC       0x55534F4EU
#define USER_CALIB_FLASH_VERSION     0x00010001U

#define USER_ULTRASONIC_TIMEOUT_US      12000U
#define USER_ULTRASONIC_MIN_VALID_US    20U
#define USER_ULTRASONIC_BLANKING_US     220U
#define USER_ULTRASONIC_FILTER_SAMPLES  9U
#define USER_ULTRASONIC_GAIN_RETRY_MAX  3U

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t point_us[USER_CALIB_POINT_COUNT];
    uint32_t reserved[2];
} UserCalibData;

typedef struct UserCalibration UserCalibration;
typedef struct UserMeasureService UserMeasureService;
typedef struct UserResultHistory UserResultHistory;
typedef struct UserInterface UserInterface;
typedef struct UserApp UserApp;

typedef uint8_t (*UserMeasureRequestFn)(void *context, UserMeasureResult *result, TickType_t timeout);
typedef void (*UserMeasureCommandFn)(void *context, const UserMeasureCommand *cmd);
typedef uint8_t (*UserMeasureReceiveFn)(void *context, UserMeasureResult *result, TickType_t timeout);

typedef struct
{
    void *context;
    UserMeasureRequestFn request_single;
    UserMeasureCommandFn send_command;
    UserMeasureReceiveFn receive_result;
} UserMeasureClient;

struct UserApp
{
    UserCalibration *calibration;
    UserMeasureService *measure;
    UserResultHistory *history;
    UserInterface *ui;
};

#ifdef __cplusplus
}
#endif

#endif /* USER_INTERNAL_H */
