/**
 * @file User/user_ui.c
 * @brief LCD/PS2 交互界面和菜单状态机模块。
 * @details
 * - 负责菜单状态、LCD 显示和 PS/2 按键事件消费。
 * - 通过依赖注入持有校准、历史和测量服务，避免跨模块直接访问内部状态。
 * - 界面层只做交互编排，测距算法和硬件控制保留在服务层。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "user_ui.h"
#include "./BSP/PS2/ps2.h"
#include "./PGA112/PGA112.h"
#include "./TFT_LCD/TFT_LCD.h"
#include "./TFT_LCD/text.h"
#include <stdio.h>
#include <string.h>

/* 全局 UI 实体单例 */
UserInterface g_user_interface;

/* ========================================================================= *
 * 内部静态函数声明（面向对象私有方法）                                        *
 * ========================================================================= */
static void UserInterface_Init(UserInterface *self);
static void UserInterface_Run(UserInterface *self);
static void UserInterface_PostKey(UserInterface *self, PS2_KeyValue_t key);

/* 局部屏幕刷新与绘制辅助函数 */
static void UserInterface_DrawMain(UserInterface *self);
static void UserInterface_ChangeMenu(UserInterface *self, uint8_t menu);
static void UserInterface_ShowString(uint16_t x, uint16_t y, const char *str);
static void UserInterface_ShowPaddedString(uint16_t x, uint16_t y, const char *str, uint8_t min_chars);
static void UserInterface_ClearWorkArea(void);
static void UserInterface_DrawWorkTitle(const char *title);
static void UserInterface_ShowLabelValue(uint8_t line, const char *label, const char *value);
static void UserInterface_ShowStatusLine(const char *text);
static PS2_KeyValue_t UserInterface_ReadKey(UserInterface *self);

/* 菜单页面状态机函数 */
static void UserInterface_MenuMeasure(UserInterface *self);
static void UserInterface_MenuCalibrate(UserInterface *self);
static void UserInterface_MenuStatus(UserInterface *self);
static void UserInterface_MenuPwmGainTest(UserInterface *self);

/* ========================================================================= *
 * 静态常量定义                                                              *
 * ========================================================================= */
/**
 * @brief 标定/校准的物理标准距离点（单位：mm）
 */
static const uint16_t k_user_calib_distance_mm[USER_CALIB_POINT_COUNT] =
{
    100U, 600U, 900U, 1300U
};

/**
 * @brief PGA112 可编程增益放大器的硬件增益寄存器编码对照表
 */
static const uint8_t k_user_gain_codes[8] =
{
    PGA112_GAIN_1,  PGA112_GAIN_2,  PGA112_GAIN_4,  PGA112_GAIN_8,
    PGA112_GAIN_16, PGA112_GAIN_32, PGA112_GAIN_64, PGA112_GAIN_128
};

#define USER_UI_LABEL_PAD_CHARS     14U
#define USER_UI_VALUE_PAD_CHARS     12U
#define USER_UI_TITLE_PAD_CHARS     28U
#define USER_UI_STATUS_PAD_CHARS    28U

/**
 * @brief UI 虚函数表常量初始化
 */
static const UserInterfaceOps k_user_interface_ops =
{
    UserInterface_Init,
    UserInterface_Run,
    UserInterface_PostKey
};

/**
 * @brief UI 模块的构造函数（依赖注入）
 * @param self 模块自身结构体指针
 * @param calibration 校准模块实例指针
 * @param history 历史记录模块实例指针
 * @param measure 测量客户端接口指针（值传递拷贝结构体）
 */
void UserInterface_Construct(UserInterface *self,
                             UserCalibration *calibration,
                             UserResultHistory *history,
                             const UserMeasureClient *measure)
{
    memset(self, 0, sizeof(*self));
    self->ops = &k_user_interface_ops; // 挂载虚表
    self->calibration = calibration;     // 注入校准依赖
    self->history = history;             // 注入历史记录依赖
    self->measure = *measure;           // 注入测量服务接口
}

/**
 * @brief UI 运行时环境初始化
 * @details 创建消息队列，清屏并初始化 LCD 图层，绘制主菜单静态框架
 */
static void UserInterface_Init(UserInterface *self)
{
    // 1. 防重复创建：为按键消息创建 FreeRTOS 队列
    if (self->key_queue == NULL)
    {
        self->key_queue = xQueueCreate(USER_KEY_QUEUE_LEN, sizeof(PS2_KeyValue_t));
    }

    // 2. 初始化显示图层属性（黑色背景，白色文字，大号字体）
    LCD_SetBackColor(Black);
    LCD_SetTextColor(White);
    LCD_SetFont(&LCD_DEFAULT_FONT, 1);

    // 3. 渲染主菜单布局
    UserInterface_DrawMain(self);
}

/**
 * @brief UI 任务状态机主循环函数
 * @note 必须在独立的 FreeRTOS 任务中被调用，会阻塞或轮询按键
 */
static void UserInterface_Run(UserInterface *self)
{
    PS2_KeyValue_t key;

    for (;;)
    {
        // 1. 读取异步按键队列
        key = UserInterface_ReadKey(self);

        // 2. 处于主菜单模式(menu == 0)时，若按下键盘的 '1' ~ '4'，触发菜单切换
        if ((self->menu == 0U) && (key >= PS2_KEY_VALUE_1) && (key <= PS2_KEY_VALUE_4))
        {
            UserInterface_ChangeMenu(self, (uint8_t)key);
        }

        // 3. 调度当前活动的菜单子状态机（各子菜单内部自带阻塞/轮询循环）
        switch (self->menu)
        {
            case 1: UserInterface_MenuMeasure(self);     break; // 实时测量
            case 2: UserInterface_MenuCalibrate(self);   break; // 标定校准
            case 3: UserInterface_MenuStatus(self);      break; // 系统状态
            case 4: UserInterface_MenuPwmGainTest(self); break; // 调试测试
            default: break;
        }

        // 4. 防止任务饥饿，释放 CPU 20ms
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief 向 UI 异步投递按键值的对外接口
 * @note 供按键扫描任务（如 Key_Task）调用，采用不等待（0延时）方式发送队列
 */
static void UserInterface_PostKey(UserInterface *self, PS2_KeyValue_t key)
{
    if (self->key_queue != NULL)
    {
        (void)xQueueSend(self->key_queue, &key, 0U);
    }
}

/**
 * @brief 绘制主界面静态分割线与菜单选项文本
 */
static void UserInterface_DrawMain(UserInterface *self)
{
    uint8_t index;
    (void)self;

    LCD_Clear(Black);
    
    // 绘制主标题与四条主框架分割线
    UserInterface_ShowString(280, 16, "Ultrasonic Meter");
    LCD_Appoint_Clear(0, 64, 800, 72, White);   // 顶部水平分割线
    LCD_Appoint_Clear(0, 440, 800, 448, White); // 底部水平分割线
    LCD_Appoint_Clear(250, 72, 252, 440, White); // 左右分栏垂直分割线

    // 绘制底部页脚信息
    UserInterface_ShowString(32, 448, "HC-SR04 + PGA112");
    UserInterface_ShowString(640, 448, "HAL RTOS");

    // 绘制左侧菜单前缀未选中状态标识 "-"
    for (index = 0U; index < 4U; index++)
    {
        UserInterface_ShowString(32, (uint16_t)(96U + 64U * index), "-");
    }

    // 绘制左侧菜单四个固定选项文本
    UserInterface_ShowString(70, 96,  "Realtime");
    UserInterface_ShowString(70, 160, "Calibrate");
    UserInterface_ShowString(70, 224, "Status");
    UserInterface_ShowString(70, 288, "PWM/Gain");
}

/**
 * @brief 切换菜单页面并更新左侧状态指示指针
 * @param menu 目标菜单 ID (1~4)，若输入 0 则退回主界面状态
 */
static void UserInterface_ChangeMenu(UserInterface *self, uint8_t menu)
{
    uint8_t index;

    // 清空右侧核心工作显示区
    UserInterface_ClearWorkArea();

    // 先重置左侧所有指针为 "-" 未选中
    for (index = 0U; index < 4U; index++)
    {
        UserInterface_ShowString(32, (uint16_t)(96U + 64U * index), "-");
    }

    // 如果切换到 1~4 页面，则将对应行的前缀改为 ">" 激活样式
    if ((menu >= 1U) && (menu <= 4U))
    {
        UserInterface_ShowString(32, (uint16_t)(96U + 64U * (menu - 1U)), ">");
        self->menu = menu;
    }
    else
    {
        self->menu = 0U; // 回到未选中任何菜单的就绪态
    }

    // 每次切换菜单时清空按键底层的键盘驱动状态机，防止历史按键抖动影响新页面
    PS2_ClearStateMachine();
}

/**
 * @brief 封装底层 LCD 字符串显示接口
 * @param x, y 渲染起始像素坐标
 * @param str 常量字符串指针
 */
static void UserInterface_ShowString(uint16_t x, uint16_t y, const char *str)
{
    // 强制限制单行显示宽度，高度为32像素，使用 GBK 字符集渲染
    LCD_ShowGBKString(x, y, (uint16_t)(LCD_Pixel_Width - x), 32U, (uint8_t *)str, 32U, 1U);
}

/**
 * @brief 固定宽度刷新：用尾部空格覆盖旧数据，避免频繁清屏闪烁
 */
static void UserInterface_ShowPaddedString(uint16_t x, uint16_t y, const char *str, uint8_t min_chars)
{
    char padded[40];
    uint8_t index = 0U;

    if (str == NULL)
    {
        str = "";
    }

    while ((str[index] != '\0') && (index < (uint8_t)(sizeof(padded) - 1U)))
    {
        padded[index] = str[index];
        index++;
    }

    while ((index < min_chars) && (index < (uint8_t)(sizeof(padded) - 1U)))
    {
        padded[index] = ' ';
        index++;
    }

    padded[index] = '\0';
    UserInterface_ShowString(x, y, padded);
}

/**
 * @brief 局部刷新：擦除右侧整个工作区域
 */
static void UserInterface_ClearWorkArea(void)
{
    LCD_Appoint_Clear(253, 73, 799, 439, Black);
}

/**
 * @brief 在右侧区绘制当前页面的标题
 */
static void UserInterface_DrawWorkTitle(const char *title)
{
    UserInterface_ShowPaddedString(280, 88, title, USER_UI_TITLE_PAD_CHARS);
}

/**
 * @brief 在指定行格式化输出一对标签和对应的值（Label-Value）
 * @param line 工作区内的相对行号（每行物理间距 30 像素）
 */
static void UserInterface_ShowLabelValue(uint8_t line, const char *label, const char *value)
{
    uint16_t y = (uint16_t)(150U + 30U * line);

    UserInterface_ShowPaddedString(280, y, label, USER_UI_LABEL_PAD_CHARS); // 左分栏显示标签
    UserInterface_ShowPaddedString(520, y, value, USER_UI_VALUE_PAD_CHARS); // 右分栏显示数值
}

/**
 * @brief 在底部固定位置刷新操作提示语或状态信息
 */
static void UserInterface_ShowStatusLine(const char *text)
{
    UserInterface_ShowPaddedString(280, 400, text, USER_UI_STATUS_PAD_CHARS);
}

/**
 * @brief 从按键队列中读取非阻塞键值
 * @return PS2_KeyValue_t 键值，若队列为空则直接返回 NULL
 */
static PS2_KeyValue_t UserInterface_ReadKey(UserInterface *self)
{
    PS2_KeyValue_t key = PS2_KEY_VALUE_NULL;

    if (self->key_queue == NULL)
    {
        return PS2_KEY_VALUE_NULL;
    }

    // 最后一个参数传入 0 确保在没有按键时不产生阻塞，从而能够快速轮询传感器数据
    (void)xQueueReceive(self->key_queue, &key, 0U);
    return key;
}

/* ========================================================================= *
 * 核心功能子菜单界面状态机实现                                              *
 * ========================================================================= */

/**
 * @brief 【菜单1】实时测距看板
 * @details 
 * 进入即发送 CONTINUOUS_START 命令让底层全速连测。
 * 内部不阻塞地一边读取最新传感器数据刷新屏幕，一边捕获 BACKSPACE 退出信号。
 */
static void UserInterface_MenuMeasure(UserInterface *self)
{
    UserMeasureCommand cmd;
    UserMeasureResult result;
    PS2_KeyValue_t key;
    char text[32];
    uint8_t timeout_status = 0U;

    // 局部宏定义：专门为了高频刷新某一行 Value 而不破坏 Label
#define MENU_VALUE_Y(line)          ((uint16_t)(150U + 30U * (line)))
#define MENU_SHOW_VALUE(line, val)  do { \
                                      uint16_t value_y = MENU_VALUE_Y(line); \
                                      UserInterface_ShowPaddedString(520, value_y, (val), USER_UI_VALUE_PAD_CHARS); \
                                  } while (0)

    // 绘制基础静态表单
    UserInterface_DrawWorkTitle("Realtime measure");
    UserInterface_ShowLabelValue(0, "Echo time(us)", "00000");
    UserInterface_ShowLabelValue(1, "Distance(mm)", "0000.0");
    UserInterface_ShowLabelValue(2, "Default(mm)", "0000.0");
    UserInterface_ShowLabelValue(3, "Calib", self->calibration->ops->is_valid(self->calibration) ? "valid" : "default");
    UserInterface_ShowLabelValue(4, "Gain(x)", "000");
    UserInterface_ShowStatusLine("Measuring, Back return");

    // 1. 发送命令激活底层测距硬件的连续采集机制
    cmd.type = USER_MEASURE_CMD_CONTINUOUS_START;
    cmd.gain_code = 0U;
    self->measure.send_command(self->measure.context, &cmd);

    // 2. 进入页面的高频局部刷新循环
    while (self->menu == 1U)
    {
        // 检测返回键退出页面
        key = UserInterface_ReadKey(self);
        if (key == PS2_KEY_VALUE_BACKSPACE)
        {
            cmd.type = USER_MEASURE_CMD_CONTINUOUS_STOP; // 停止连续测量以节能
            self->measure.send_command(self->measure.context, &cmd);
            UserInterface_ChangeMenu(self, 0U);          // 回到主菜单
            break;
        }

        // 3. 轮询接收最新数据，超时时间设定为 30ms（契合超声波回波频率）
        if (self->measure.receive_result(self->measure.context, &result, pdMS_TO_TICKS(30)) != 0U)
        {
            if (result.valid != 0U) // 如果数据有效
            {
                snprintf(text, sizeof(text), "%05lu", (unsigned long)result.echo_us);
                MENU_SHOW_VALUE(0, text);
                snprintf(text, sizeof(text), "%06.1f", (double)result.distance_mm);
                MENU_SHOW_VALUE(1, text);
                snprintf(text, sizeof(text), "%06.1f", (double)result.default_distance_mm);
                MENU_SHOW_VALUE(2, text);
                snprintf(text, sizeof(text), "%03u", PGA112_GetGainValue(result.gain_code));
                MENU_SHOW_VALUE(4, text);

                // 错误状态恢复提示
                if (timeout_status != 0U)
                {
                    UserInterface_ShowStatusLine("Measuring, Back return");
                    timeout_status = 0U;
                }
            }
            else // 底层硬件返回数据，但标记为超时未捕获回波
            {
                snprintf(text, sizeof(text), "%03u", PGA112_GetGainValue(result.gain_code));
                MENU_SHOW_VALUE(4, text);

                if (timeout_status == 0U)
                {
                    UserInterface_ShowStatusLine("Measure timeout");
                    timeout_status = 1U;
                }
            }
        }
    }

#undef MENU_SHOW_VALUE
#undef MENU_VALUE_Y
}

/**
 * @brief 【菜单2】4点手动标定/校准向导页面
 * @details 
 * 用户将挡板移至提示的标准毫米位置（100, 600, 900, 1300mm），按下 ENTER 触发单次强校准采样。
 * 4点全部集齐后，自动将其写入 Flash 进行持久化保存并退回主菜单。
 */
static void UserInterface_MenuCalibrate(UserInterface *self)
{
    UserCalibData new_calib;
    UserMeasureResult result;
    PS2_KeyValue_t key;
    uint8_t step = 0U;
    char text[32];

    // 初始化新的校准结构体包头与版本校验码
    memset(&new_calib, 0, sizeof(new_calib));
    new_calib.magic = USER_CALIB_FLASH_MAGIC;
    new_calib.version = USER_CALIB_FLASH_VERSION;

    UserInterface_DrawWorkTitle("Calibrate");
    UserInterface_ShowLabelValue(0, "Target(mm)", "0100");
    UserInterface_ShowLabelValue(1, "State", "waiting");
    UserInterface_ShowLabelValue(2, "100mm(us)", "00000");
    UserInterface_ShowLabelValue(3, "600mm(us)", "00000");
    UserInterface_ShowLabelValue(4, "900mm(us)", "00000");
    UserInterface_ShowLabelValue(5, "1300mm(us)", "00000");
    UserInterface_ShowLabelValue(6, "Result", "pending");
    UserInterface_ShowStatusLine("Enter sample, Back return");

    while (self->menu == 2U)
    {
        key = UserInterface_ReadKey(self);
        if (key == PS2_KEY_VALUE_BACKSPACE)
        {
            UserInterface_ChangeMenu(self, 0U);
            break;
        }

        // 当用户按下确认键，代表挡板已对齐，开始采样
        if (key == PS2_KEY_VALUE_ENTER)
        {
            snprintf(text, sizeof(text), "%04u", k_user_calib_distance_mm[step]);
            UserInterface_ShowLabelValue(0, "Target(mm)", text);
            UserInterface_ShowLabelValue(1, "State", "sampling");

            // 发起长超时（2.5秒）的单次精准测量请求，等待回波波形
            if ((self->measure.request_single(self->measure.context, &result, pdMS_TO_TICKS(2500)) != 0U) &&
                (result.valid != 0U))
            {
                // 保存该点传感器捕获到的原始时间值（微秒数）
                new_calib.point_us[step] = result.echo_us;
                snprintf(text, sizeof(text), "%05lu", (unsigned long)result.echo_us);
                
                // 将对应采样点的 us 数据刷新到屏幕对应行
                UserInterface_ShowLabelValue((uint8_t)(2U + step), "Sample(us)", text);
                UserInterface_ShowLabelValue(1, "State", "success");
                step++; // 向下一个点迈进

                // 4个点全满，校准数据集齐
                if (step >= USER_CALIB_POINT_COUNT)
                {
                    // 调用保存虚接口将结果写入硬件 Flash 中
                    if (self->calibration->ops->save(self->calibration, &new_calib) != 0U)
                    {
                        UserInterface_ShowLabelValue(6, "Result", "saved");
                    }
                    else
                    {
                        UserInterface_ShowLabelValue(6, "Result", "failed");
                    }

                    vTaskDelay(pdMS_TO_TICKS(1000)); // 驻留显示1秒
                    UserInterface_ChangeMenu(self, 0U); // 强制安全退回主菜单
                    break;
                }

                // 提前显示下一个要对齐的目标物理距离提示
                snprintf(text, sizeof(text), "%04u", k_user_calib_distance_mm[step]);
                UserInterface_ShowLabelValue(0, "Target(mm)", text);
            }
            else
            {
                UserInterface_ShowLabelValue(1, "State", "timeout"); // 测量超时或未击中挡板
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief 【菜单3】静态系统状态查看看板
 * @details 
 * 展示当前的 Flash 4点标定微秒数据、增益等级、以及历史记录条数。
 * 允许用户在此界面按 ENTER 键触发一次非连续的“单次打点测量”来捕捉当前回波。
 */
static void UserInterface_MenuStatus(UserInterface *self)
{
    const UserCalibData *calib = self->calibration->ops->data(self->calibration);
    UserMeasureResult result;
    PS2_KeyValue_t key;
    char text[32];

    UserInterface_DrawWorkTitle("Status");
    UserInterface_ShowLabelValue(0, "Calib", self->calibration->ops->is_valid(self->calibration) ? "valid" : "default");

    // 一次性渲染当前保存在系统里的 4 点校准常量值
    snprintf(text, sizeof(text), "%05lu", (unsigned long)calib->point_us[0]);
    UserInterface_ShowLabelValue(1, "Point0(us)", text);
    snprintf(text, sizeof(text), "%05lu", (unsigned long)calib->point_us[1]);
    UserInterface_ShowLabelValue(2, "Point1(us)", text);
    snprintf(text, sizeof(text), "%05lu", (unsigned long)calib->point_us[2]);
    UserInterface_ShowLabelValue(3, "Point2(us)", text);
    snprintf(text, sizeof(text), "%05lu", (unsigned long)calib->point_us[3]);
    UserInterface_ShowLabelValue(4, "Point3(us)", text);

    // 读取放大器当前硬件寄存器内的增益倍数
    snprintf(text, sizeof(text), "%03u", PGA112_GetGainValue(PGA112_GetGainCode()));
    UserInterface_ShowLabelValue(5, "Gain(x)", text);
    
    // 获取 EEPROM 或 内存中已存的历史记录数据笔数
    snprintf(text, sizeof(text), "%02u", self->history->ops->count(self->history));
    UserInterface_ShowLabelValue(6, "History", text);
    UserInterface_ShowLabelValue(7, "Last echo(us)", "00000");
    UserInterface_ShowStatusLine("Enter single, Back return");

    while (self->menu == 3U)
    {
        key = UserInterface_ReadKey(self);
        if (key == PS2_KEY_VALUE_BACKSPACE)
        {
            UserInterface_ChangeMenu(self, 0U);
            break;
        }

        // 按 ENTER 执行单次临时手动作业
        if (key == PS2_KEY_VALUE_ENTER)
        {
            if ((self->measure.request_single(self->measure.context, &result, pdMS_TO_TICKS(2500)) != 0U) &&
                (result.valid != 0U))
            {
                snprintf(text, sizeof(text), "%05lu", (unsigned long)result.echo_us);
                UserInterface_ShowLabelValue(7, "Last echo(us)", text);
                
                // 将计算出的物理距离临时丢在底部状态栏进行显示
                snprintf(text, sizeof(text), "%06.1f", (double)result.distance_mm);
                UserInterface_ShowStatusLine(text);
                
                // 重新刷新可能发生变化的历史数据计数
                snprintf(text, sizeof(text), "%02u", self->history->ops->count(self->history));
                UserInterface_ShowLabelValue(6, "History", text);
            }
            else
            {
                UserInterface_ShowStatusLine("Single timeout");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief 【菜单4】超声波硬件测试与 PGA 增益手动步进修改页面
 * @details 
 * 进入后引脚将持续发出 40kHz 超声波方波脉冲（硬件PD12/PD13）。
 * 用户可以通过键盘的 '+' / '-' 键实时在线跨级修改 PGA112 放大器的放大增益系数（1倍~128倍）。
 */
static void UserInterface_MenuPwmGainTest(UserInterface *self)
{
    UserMeasureCommand cmd;
    PS2_KeyValue_t key;
    uint8_t gain_index = PGA112_DEFAULT_GAIN_CODE; // 起始硬件默认倍率索引
    char text[32];

    UserInterface_DrawWorkTitle("PWM/Gain test");
    UserInterface_ShowLabelValue(0, "PWM output", "on");
    UserInterface_ShowLabelValue(1, "Freq(Hz)", "40000"); // 经典 40kHz 超声波换能器频率
    UserInterface_ShowLabelValue(2, "Duty(%)", "50");     // 50% 占空比标准方波
    snprintf(text, sizeof(text), "%03u", PGA112_GetGainValue(k_user_gain_codes[gain_index]));
    UserInterface_ShowLabelValue(3, "Gain(x)", text);
    UserInterface_ShowLabelValue(4, "Pins", "PD12/PD13"); // 显示底层硬件 PWM 输出引脚配置
    UserInterface_ShowStatusLine("+/- gain, Back stop");

    // 1. 强开底层的 PWM 全速持续输出机制
    cmd.type = USER_MEASURE_CMD_PWM_ENABLE;
    cmd.gain_code = 0U;
    self->measure.send_command(self->measure.context, &cmd);

    // 2. 将当前增益立即设置并同步进硬件放大器芯片
    cmd.type = USER_MEASURE_CMD_GAIN_SET;
    cmd.gain_code = k_user_gain_codes[gain_index];
    self->measure.send_command(self->measure.context, &cmd);

    while (self->menu == 4U)
    {
        key = UserInterface_ReadKey(self);
        
        // 捕获返回，离开前必须显式发送命令强关 PWM
        if (key == PS2_KEY_VALUE_BACKSPACE)
        {
            cmd.type = USER_MEASURE_CMD_PWM_DISABLE;
            self->measure.send_command(self->measure.context, &cmd);
            UserInterface_ChangeMenu(self, 0U);
            break;
        }
        // 按 '+' 键升高放大增益（上限 128 倍）
        else if ((key == PS2_KEY_VALUE_ADD) && (gain_index < 7U))
        {
            gain_index++;
            cmd.type = USER_MEASURE_CMD_GAIN_SET;
            cmd.gain_code = k_user_gain_codes[gain_index];
            self->measure.send_command(self->measure.context, &cmd); // 下发 SPI/I2C 增益调节配置
            
            snprintf(text, sizeof(text), "%03u", PGA112_GetGainValue(k_user_gain_codes[gain_index]));
            UserInterface_ShowLabelValue(3, "Gain(x)", text);
        }
        // 按 '-' 键降低放大增益（下限 1 倍）
        else if ((key == PS2_KEY_VALUE_MINUS) && (gain_index > 0U))
        {
            gain_index--;
            cmd.type = USER_MEASURE_CMD_GAIN_SET;
            cmd.gain_code = k_user_gain_codes[gain_index];
            self->measure.send_command(self->measure.context, &cmd);
            
            snprintf(text, sizeof(text), "%03u", PGA112_GetGainValue(k_user_gain_codes[gain_index]));
            UserInterface_ShowLabelValue(3, "Gain(x)", text);
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
