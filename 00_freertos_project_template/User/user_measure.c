/**
 * @file User/user_measure.c
 * @brief 超声测距服务状态机、PWM 触发和回波采集模块。
 * @details
 * - 封装测量命令队列、结果队列、任务通知和中断共享状态。
 * - UI 通过客户端接口调用测量能力，不直接依赖 PWM、PGA 或 EXTI 细节。
 * - 中断路径只记录时间戳和完成标志，耗时处理交给测量任务。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "user_measure.h"
#include "./BSP/EXTI/exti.h"
#include "./BSP/TIMER/ultrasonic_timer.h"
#include "./PGA112/PGA112.h"
#include <string.h>

UserMeasureService g_user_measure_service;

static void UserMeasureService_Init(UserMeasureService *self);
static void UserMeasureService_Run(UserMeasureService *self);
static void UserMeasureService_SendCommand(UserMeasureService *self, const UserMeasureCommand *cmd);
static uint8_t UserMeasureService_RequestSingle(UserMeasureService *self,
                                                UserMeasureResult *result,
                                                TickType_t timeout);
static uint8_t UserMeasureService_ReceiveResult(UserMeasureService *self,
                                                UserMeasureResult *result,
                                                TickType_t timeout);
static uint8_t UserMeasureService_IsBusy(const UserMeasureService *self);

static void UserMeasureService_HandleCommand(UserMeasureService *self,
                                             const UserMeasureCommand *cmd,
                                             uint8_t *continuous);
static uint8_t UserMeasureService_DoMeasurement(UserMeasureService *self,
                                                UserMeasureResult *result,
                                                TickType_t timeout);
static void UserMeasureService_StartCapture(UserMeasureService *self);
static void UserMeasureService_FillResult(UserMeasureService *self,
                                          UserMeasureResult *result,
                                          uint8_t valid,
                                          uint32_t echo_us);
static void UserMeasureService_PublishResult(UserMeasureService *self,
                                             const UserMeasureResult *result);
static void UserMeasureService_ApplyGain(UserMeasureService *self, uint8_t gain_code);
static uint8_t UserMeasureService_SelectGain(uint32_t echo_us);

static const UserMeasureServiceOps s_user_measure_ops =
{
    UserMeasureService_Init,
    UserMeasureService_Run,
    UserMeasureService_SendCommand,
    UserMeasureService_RequestSingle,
    UserMeasureService_ReceiveResult,
    UserMeasureService_IsBusy
};

void UserMeasureService_Construct(UserMeasureService *self,
                                  UserCalibration *calibration,
                                  UserResultHistory *history)
{
    if (self == NULL)
    {
        return;
    }

    memset(self, 0, sizeof(*self));
    self->ops = &s_user_measure_ops;
    self->calibration = calibration;
    self->history = history;
    self->gain_code = PGA112_DEFAULT_GAIN_CODE;
}

static void UserMeasureService_Init(UserMeasureService *self)
{
    if (self == NULL)
    {
        return;
    }

    if (self->cmd_queue == NULL)
    {
        self->cmd_queue = xQueueCreate(USER_MEASURE_QUEUE_LEN, sizeof(UserMeasureCommand));
    }

    if (self->result_queue == NULL)
    {
        self->result_queue = xQueueCreate(USER_RESULT_QUEUE_LEN, sizeof(UserMeasureResult));
    }

    Ultrasonic_PWM_Init();
    ultrasonic_timebase_init();
    ultrasonic_echo_exti_init();
    ultrasonic_echo_exti_register_callback(UserMeasureService_EchoCallback);

    PGA112_Init();
    UserMeasureService_ApplyGain(self, PGA112_DEFAULT_GAIN_CODE);
}

static void UserMeasureService_Run(UserMeasureService *self)
{
    UserMeasureCommand cmd;
    UserMeasureResult result;
    uint8_t continuous = 0U;

    if (self == NULL)
    {
        vTaskDelete(NULL);
    }

    for (;;)
    {
        /* 命令队列是 UI/业务层到测量任务的唯一控制入口。
         * 20ms 的短等待让任务在“无命令但连续测量开启”时仍能按固定节奏推进采集。
         */
        if ((self->cmd_queue != NULL) &&
            (xQueueReceive(self->cmd_queue, &cmd, pdMS_TO_TICKS(20U)) == pdTRUE))
        {
            UserMeasureService_HandleCommand(self, &cmd, &continuous);
        }

        if (continuous != 0U)
        {
            /* 连续模式不直接阻塞 UI：结果通过 result_queue 覆盖式发布，
             * UI 只读取最新一帧，避免历史积压造成屏幕显示滞后。
             */
            (void)UserMeasureService_DoMeasurement(self, &result, pdMS_TO_TICKS(80U));
            vTaskDelay(pdMS_TO_TICKS(50U));
        }
    }
}

static void UserMeasureService_SendCommand(UserMeasureService *self, const UserMeasureCommand *cmd)
{
    if ((self == NULL) || (cmd == NULL) || (self->cmd_queue == NULL))
    {
        return;
    }

    if ((cmd->type == USER_MEASURE_CMD_CONTINUOUS_START) && (self->result_queue != NULL))
    {
        xQueueReset(self->result_queue);
    }

    (void)xQueueSend(self->cmd_queue, cmd, 0U);
}

static uint8_t UserMeasureService_RequestSingle(UserMeasureService *self,
                                                UserMeasureResult *result,
                                                TickType_t timeout)
{
    if ((self == NULL) || (result == NULL))
    {
        return 0U;
    }

    return UserMeasureService_DoMeasurement(self, result, timeout);
}

static uint8_t UserMeasureService_ReceiveResult(UserMeasureService *self,
                                                UserMeasureResult *result,
                                                TickType_t timeout)
{
    if ((self == NULL) || (result == NULL) || (self->result_queue == NULL))
    {
        return 0U;
    }

    return (xQueueReceive(self->result_queue, result, timeout) == pdTRUE) ? 1U : 0U;
}

static uint8_t UserMeasureService_IsBusy(const UserMeasureService *self)
{
    if (self == NULL)
    {
        return 0U;
    }

    return (uint8_t)((self->busy != 0U) || (ultrasonic_pwm_is_active() != 0U));
}

static void UserMeasureService_HandleCommand(UserMeasureService *self,
                                             const UserMeasureCommand *cmd,
                                             uint8_t *continuous)
{
    UserMeasureResult result;

    if ((self == NULL) || (cmd == NULL) || (continuous == NULL))
    {
        return;
    }

    switch (cmd->type)
    {
        case USER_MEASURE_CMD_SINGLE:
            (void)UserMeasureService_DoMeasurement(self, &result, pdMS_TO_TICKS(120U));
            break;

        case USER_MEASURE_CMD_CONTINUOUS_START:
            *continuous = 1U;
            break;

        case USER_MEASURE_CMD_CONTINUOUS_STOP:
            *continuous = 0U;
            Ultrasonic_PWM_OutputDisable();
            self->busy = 0U;
            self->measure_active = 0U;
            break;

        case USER_MEASURE_CMD_GAIN_SET:
            UserMeasureService_ApplyGain(self, cmd->gain_code);
            break;

        case USER_MEASURE_CMD_PWM_ENABLE:
            Ultrasonic_PWM_OutputEnable();
            self->busy = 1U;
            break;

        case USER_MEASURE_CMD_PWM_DISABLE:
            Ultrasonic_PWM_OutputDisable();
            self->busy = 0U;
            break;

        default:
            break;
    }
}

static uint8_t UserMeasureService_DoMeasurement(UserMeasureService *self,
                                                UserMeasureResult *result,
                                                TickType_t timeout)
{
    uint32_t notified;
    uint32_t echo_us;
    uint8_t valid;
    uint8_t next_gain;

    if ((self == NULL) || (result == NULL))
    {
        return 0U;
    }

    if (self->busy != 0U)
    {
        UserMeasureService_FillResult(self, result, 0U, 0U);
        return 0U;
    }

    /* 清空可能残留的任务通知，防止上一轮 ISR 通知误唤醒本轮测量。 */
    (void)ulTaskNotifyTake(pdTRUE, 0U);
    self->waiting_task = xTaskGetCurrentTaskHandle();
    UserMeasureService_StartCapture(self);

    /* 任务在这里等待回波下降沿 ISR 唤醒。
     * 这样测量期间不忙等 CPU，同时 ISR 只负责记录时间戳和通知任务。
     */
    notified = ulTaskNotifyTake(pdTRUE, timeout);
    self->waiting_task = NULL;

    echo_us = self->echo_time_us;
    valid = (uint8_t)((notified != 0U) &&
                      (echo_us >= USER_ULTRASONIC_MIN_VALID_US) &&
                      (echo_us <= USER_ULTRASONIC_TIMEOUT_US));

    if (valid == 0U)
    {
        /* 超时或非法脉宽时主动收尾，避免 busy/measure_active 卡死影响低功耗和下一次测量。 */
        self->measure_active = 0U;
        self->busy = 0U;
        Ultrasonic_PWM_OutputDisable();
    }

    UserMeasureService_FillResult(self, result, valid, echo_us);
    UserMeasureService_PublishResult(self, result);

    if (valid != 0U)
    {
        next_gain = UserMeasureService_SelectGain(echo_us);
        if (next_gain != self->gain_code)
        {
            UserMeasureService_ApplyGain(self, next_gain);
        }
    }

    return valid;
}

static void UserMeasureService_StartCapture(UserMeasureService *self)
{
    /* 发射前一次性重置 ISR 共享状态。
     * 临界区保证 EXTI 在状态半初始化时不会读到不一致的 rise/active/busy 组合。
     */
    taskENTER_CRITICAL();
    self->measure_active = 1U;
    self->echo_rise_seen = 0U;
    self->echo_rise_us = 0U;
    self->echo_time_us = 0U;
    self->busy = 1U;
    taskEXIT_CRITICAL();

    ultrasonic_timebase_reset();
    /* FireBurst 会发出有限个 PWM 周期，随后硬件/驱动切回接收窗口。 */
    Ultrasonic_FireBurst();
}

static void UserMeasureService_FillResult(UserMeasureService *self,
                                          UserMeasureResult *result,
                                          uint8_t valid,
                                          uint32_t echo_us)
{
    result->valid = valid;
    result->echo_us = echo_us;
    result->gain_code = self->gain_code;

    if ((valid != 0U) && (self->calibration != NULL))
    {
        result->distance_mm = self->calibration->ops->to_distance(self->calibration, echo_us);
        result->default_distance_mm = self->calibration->ops->to_default_distance(echo_us);
        self->last_echo_us = echo_us;
    }
    else
    {
        result->distance_mm = 0.0f;
        result->default_distance_mm = 0.0f;
    }
}

static void UserMeasureService_PublishResult(UserMeasureService *self,
                                             const UserMeasureResult *result)
{
    if ((self == NULL) || (result == NULL))
    {
        return;
    }

    if ((result->valid != 0U) && (self->history != NULL))
    {
        self->history->ops->push(self->history, result);
    }

    if (self->result_queue != NULL)
    {
        (void)xQueueOverwrite(self->result_queue, result);
    }
}

static void UserMeasureService_ApplyGain(UserMeasureService *self, uint8_t gain_code)
{
    if (self == NULL)
    {
        return;
    }

    if (gain_code > PGA112_GAIN_128)
    {
        gain_code = PGA112_GAIN_128;
    }

    self->gain_code = gain_code;
    self->gain_settle_discard = 1U;
    PGA112_SetGainCode(gain_code);
}

static uint8_t UserMeasureService_SelectGain(uint32_t echo_us)
{
    /* 简单分段自动增益策略：
     * 近距离回波强，使用低增益避免饱和；远距离回波弱，逐步提高 PGA112 增益。
     * 阈值按回波时间而非距离判断，可避免浮点换算进入实时控制路径。
     */
    if (echo_us < 180U)  return PGA112_GAIN_1;
    if (echo_us < 700U)  return PGA112_GAIN_2;
    if (echo_us < 1400U) return PGA112_GAIN_4;
    if (echo_us < 2600U) return PGA112_GAIN_8;
    if (echo_us < 4500U) return PGA112_GAIN_16;
    if (echo_us < 7500U) return PGA112_GAIN_32;
    return PGA112_GAIN_64;
}

void UserMeasureService_EchoCallback(uint32_t now_us, GPIO_PinState level)
{
    UserMeasureService *self = &g_user_measure_service;
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (self->measure_active == 0U)
    {
        return;
    }

    if (level != GPIO_PIN_RESET)
    {
        /* 上升沿表示回波开始。
         * blanking 窗口用于屏蔽刚发射后的换能器振铃和电气串扰。
         */
        if (now_us >= USER_ULTRASONIC_BLANKING_US)
        {
            self->echo_rise_us = now_us;
            self->echo_rise_seen = 1U;
        }
        return;
    }

    if (self->echo_rise_seen != 0U)
    {
        /* 下降沿表示回波结束，脉宽就是可用于距离换算的飞行时间窗口。
         * 只在捕获到合法上升沿后处理下降沿，避免噪声低电平导致负脉宽。
         */
        self->echo_time_us = now_us - self->echo_rise_us;
        self->measure_active = 0U;
        self->busy = 0U;

        if (self->waiting_task != NULL)
        {
            vTaskNotifyGiveFromISR(self->waiting_task, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
    }
}
