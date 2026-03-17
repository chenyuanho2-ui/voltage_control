/**
 * @file instruction_manager.c
 * @brief 指令系统核心管理器
 * * 职责：
 * 1. 作为指令解析的总入口，协调 Basic, Ramp, Timer, Cal 各模块的解析逻辑。
 * 2. 维护系统全局状态（正常模式/校准模式）并管理当前转速变量。
 * 3. 提供统一的硬件输出接口 Apply_Physical_Output，实现 20ms 定时 DAC 刷新。
 * 4. 提供自定义字符串比较函数 custom_stricmp 以解决编译器库函数兼容性问题。
 */


#include "instruction_manager.h"
#include "cmd_basic.h"
#include "cmd_ramp.h"
#include "cmd_timer.h"
#include "cmd_cal.h"
#include "calibration.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

//新增引用
#include "cmd_help.h"
#include "cmd_direction.h"
#include "cmd_start.h"
#include "cmd_adc.h"
#include "cmd_buzzer.h"
#include "cmd_self_test.h"

static float current_speed = 0.0f;
static SystemMode_t global_mode = MODE_NORMAL;

// 统一硬件输出函数
static void Apply_Physical_Output(float speed) {
    float val = (global_mode == MODE_CALIBRATION) ? speed : Calibration_GetCorrectedValue(speed);
    uint16_t dac_val = (uint16_t)(val * 40.95f);
    if (dac_val > 4095) dac_val = 4095;
    
    extern void MCP4725_SetVoltage(uint16_t output, uint8_t writeEEPROM);
    MCP4725_SetVoltage(dac_val, 0);
}

///////////新增初始化
void Instruction_Init(void) {
    current_speed = 0.0f;
    global_mode = MODE_NORMAL;
    Calibration_Init(); // 初始化校准
	Direction_Init(); // 初始化方向引脚
    Start_Init();     // 初始化启停引脚
	Buzzer_Init();      // 初始化蜂鸣器
	
    Apply_Physical_Output(0);
}

void Instruction_Parse(char* cmd) {
    char mapped_cmd[32]; 

    // 1. 去除原指令中的回车换行符
    char* p = strchr(cmd, '\r');
    if(p) *p = '\0';
    p = strchr(cmd, '\n');
    if(p) *p = '\0';

    // 2. 默认将原指令安全地拷贝进缓存
    strncpy(mapped_cmd, cmd, sizeof(mapped_cmd) - 1);
    mapped_cmd[sizeof(mapped_cmd) - 1] = '\0';

    // 3. 构建快捷键映射表 (预留8个快捷键位置)
    typedef struct {
        const char* key;     // 快捷键字符
        const char* target;  // 实际替换的指令
    } Shortcut_t;
    
    // 你可以在这里极其方便地增加或修改快捷键
    Shortcut_t shortcuts[8] = {
        {"q", "on"},     // 快捷键1：发送 q 执行 on
        {"w", "off"},    // 快捷键2：发送 w 执行 off
        {"e", "bt15"},   // 快捷键3：发送 e 执行 bt15
        {"",  ""},       // 快捷键4 (预留，格式如 {"r", "50>100t5"})
        {"",  ""},       // 快捷键5 (预留)
        {"",  ""},       // 快捷键6 (预留)
        {"",  ""},       // 快捷键7 (预留)
        {"",  ""}        // 快捷键8 (预留)
    };

    // 遍历映射表进行拦截与替换 (使用 custom_stricmp 支持大小写混用，发 Q 和 q 都可以)
    for (int i = 0; i < 8; i++) {
        // 如果按键被定义了，且匹配当前接收到的指令
        if (shortcuts[i].key[0] != '\0' && custom_stricmp(mapped_cmd, shortcuts[i].key) == 0) {
            strcpy(mapped_cmd, shortcuts[i].target); // 将内容替换为目标指令
            break; // 匹配成功，直接跳出循环
        }
    }

    // 打印实际执行的指令，方便调试
    printf("CMD: %s\r\n", mapped_cmd);

    // 4. 处理校准模式切换
    if (custom_stricmp(mapped_cmd, "ci") == 0) {
        global_mode = MODE_CALIBRATION;
        Cal_Start();
        return;
    }
    if (custom_stricmp(mapped_cmd, "co") == 0) {
        Cal_End();
        global_mode = MODE_NORMAL;
        return;
    }

    // 5. 根据模式分发解析任务 (后续全部使用 mapped_cmd 进行解析)
    if (Instruction_GetMode() == MODE_CALIBRATION) {
        if (SelfTest_Parse(mapped_cmd)) return; 
        Cal_Process(mapped_cmd);
    } else {
        if (Help_Parse(mapped_cmd))       return; 
        if (ADC_Parse(mapped_cmd))        return; 
        if (Buzzer_Parse(mapped_cmd))     return; 
        if (Start_Parse(mapped_cmd))      return; 
        if (Direction_Parse(mapped_cmd))  return; 
        if (Ramp_Parse(mapped_cmd))       return;
        if (Timer_Parse(mapped_cmd))      return;
        if (Basic_Parse(mapped_cmd))      return;
    }
}

void Instruction_Loop(void) {
    uint32_t now = HAL_GetTick();
    if (global_mode == MODE_NORMAL) {
        Ramp_Update(now);
        Timer_Update(now);
		ADC_Update(now); // 新增 ADC 5Hz 打印处理
		Buzzer_Update(now); // 新增蜂鸣器定时更新
    }

    // 每 20ms 刷新一次 DAC
    static uint32_t last_tick = 0;
    if (now - last_tick >= 20) {
        Apply_Physical_Output(current_speed);
        last_tick = now;
    }
}

// Getters & Setters
void Instruction_SetSpeed(float speed) { current_speed = speed; }
float Instruction_GetSpeed(void) { return current_speed; }
void Instruction_SetMode(SystemMode_t mode) { global_mode = mode; }
SystemMode_t Instruction_GetMode(void) { return global_mode; }

// 自定义实现不区分大小写的字符串比较
int custom_stricmp(const char *s1, const char *s2) {
    while (*s1 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2))) {
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
