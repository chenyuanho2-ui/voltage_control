/*
 * instruction.c
 */
 
 /**
 * ==============================================================================
 * 控制指令说明 (Instruction Manual)
 * ==============================================================================
 * * 1. 基础调速 (Direct Set):
 * - [数值]          : 立即设置转速 (例如: "50" -> 立即变更为 50% 速度)
 * * 2. 线性渐变 (Ramp):
 * - [A]>[B]t[S]     : 在 S 秒内从 A% 渐变到 B% (例如: "20>80t10" -> 10秒从20%升到80%)
 * - [A]>[B]         : 缺省时间，默认 10 秒渐变
 * * 3. 快捷增减 (Speed Up/Down):
 * - [A]+t[S]        : 在 S 秒内从 A% 升至 100% (例如: "50+t5")
 * - [A]-t[S]        : 在 S 秒内从 A% 降至 0%   (例如: "50-t20")
 * - [A]+ / [A]-     : 缺省时间，默认 10 秒
 * * 4. 定时停止 (Timer):
 * - [A]s[S]         : 以 A% 速度运行，S 秒后停止 (例如: "50s15")
 * - s[S]            : 保持当前速度，S 秒后停止   (例如: "s15")
 * * 5. 运行控制 (Operation):
 * - s (或 S)        : 急停 (Emergency Stop) -> 转速立即归零，清除所有任务
 * - p (或 P)        : 暂停/恢复 (Pause/Resume) -> 冻结当前渐变或计时过程
 * * 提示：所有数值范围为 0.0 - 100.0，指令不区分大小写。
 **- 6. 在线校准 
 --- ci 			 ：进入校准模式
 --- co	             ：完成校准模式
 * ==============================================================================
 */
/*
 * instruction.c
 */
 
#include "instruction.h"
#include "main.h"      
#include "calibration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// --- 外部依赖声明 ---
extern void MCP4725_SetVoltage(uint16_t output, uint8_t writeEEPROM);

// --- 内部变量 ---
static float current_set_speed = 0.0f; 

// 状态标志
static uint8_t is_paused = 0;           
static uint32_t pause_start_tick = 0;   

// --- 校准模式状态机 ---
typedef enum {
    MODE_NORMAL = 0,
    MODE_CAL_WAIT_CMD,    // 等待用户输入指令值(0-100)
    MODE_CAL_WAIT_MEASURE // 等待用户输入实际测量值
} ControlMode_t;

static ControlMode_t sys_mode = MODE_NORMAL;
static float cal_pending_cmd = 0.0f; // 暂存当前的指令值

// 渐变控制结构体
typedef struct {
    uint8_t active;       
    float start_val;      
    float end_val;        
    uint32_t start_tick;  
    uint32_t duration_ms; 
} Ramp_t;

// 定时停止结构体
typedef struct {
    uint8_t active;       
    uint32_t start_tick;  
    uint32_t delay_ms;    
} Timer_t;

static Ramp_t ramp = {0};
static Timer_t stop_timer = {0};

// --- 内部函数：执行最终的硬件设置 ---

// 1. 应用修正后的速度 (正常模式用)
static void Apply_Speed(float speed) {
    if (speed < 0.0f) speed = 0.0f;
    if (speed > 100.0f) speed = 100.0f;

    float corrected = Calibration_GetCorrectedValue(speed);
    uint16_t dac_val = (uint16_t)(corrected * 40.95f);
    if (dac_val > 4095) dac_val = 4095;

    MCP4725_SetVoltage(dac_val, 0);
}

// 2. 应用原始指令值 (校准模式用，不经过校准算法)
static void Apply_Raw_Speed(float raw_cmd) {
    if (raw_cmd < 0.0f) raw_cmd = 0.0f;
    if (raw_cmd > 100.0f) raw_cmd = 100.0f;

    uint16_t dac_val = (uint16_t)(raw_cmd * 40.95f);
    if (dac_val > 4095) dac_val = 4095;

    MCP4725_SetVoltage(dac_val, 0);
}

void Instruction_Init(void) {
    Calibration_Init(); // 初始化校准数据
    current_set_speed = 0.0f;
    ramp.active = 0;
    stop_timer.active = 0;
    is_paused = 0;
    sys_mode = MODE_NORMAL;
    Apply_Speed(0.0f);
}

// 辅助函数：解析时间参数
static float Parse_Time_Suffix(char* str, float default_val) {
    if (str && *str == 't') {
        return atof(str + 1);
    }
    return default_val;
}

void Instruction_Parse(char* cmd) {
    // 移除换行符
    char* p = strchr(cmd, '\r');
    if(p) *p = '\0';
    p = strchr(cmd, '\n');
    if(p) *p = '\0';

    float val1 = 0.0f, val2 = 0.0f, time_sec = 0.0f;
    char* ptr;

    printf("CMD: %s\r\n", cmd);

    // --- 0. 全局通用指令 ---
    
    // [急停 / 退出校准] "s"
    if (strcasecmp(cmd, "s") == 0) {
        if (sys_mode != MODE_NORMAL) {
            sys_mode = MODE_NORMAL;
            printf(">> Exit Calibration Mode.\r\n");
        } else {
            is_paused = 0;
            ramp.active = 0;
            stop_timer.active = 0;
        }
        current_set_speed = 0.0f;
        Apply_Speed(0.0f);
        printf(">> STOP (0)\r\n");
        return;
    }

    // --- 1. 校准模式逻辑 ---
    
    // 进入校准模式 "ci"
    if (strcasecmp(cmd, "ci") == 0) {
        sys_mode = MODE_CAL_WAIT_CMD;
        Calibration_Start();
        Apply_Raw_Speed(0.0f);
        printf(">> [CAL MODE START]\r\n");
        printf(">> Please input Command Value (0-100):\r\n");
        return;
    }

    // 完成校准 "co"
    if (strcasecmp(cmd, "co") == 0) {
        if (sys_mode == MODE_NORMAL) {
            printf(">> Error: Not in cal mode.\r\n");
            return;
        }
        int count = Calibration_End();
        sys_mode = MODE_NORMAL;
        Apply_Speed(0.0f);
        printf(">> [CAL DONE] Updated %d points.\r\n", count);
        return;
    }

    // 处理校准模式下的数值输入
    if (sys_mode != MODE_NORMAL) {
        float val = atof(cmd);
        
        if (sys_mode == MODE_CAL_WAIT_CMD) {
            // 用户输入了想要的指令值 (Command)
            cal_pending_cmd = val;
            Apply_Raw_Speed(val); // 直接输出电压，不修正
            sys_mode = MODE_CAL_WAIT_MEASURE;
            printf(">> Set DAC: %.1f%%. Measure the speed, then input ACTUAL value:\r\n", val);
        } 
        else if (sys_mode == MODE_CAL_WAIT_MEASURE) {
            // 用户输入了看到的实际值 (Measured)
            Calibration_AddPoint(cal_pending_cmd, val);
            sys_mode = MODE_CAL_WAIT_CMD;
            printf(">> Recorded: Cmd=%.1f, Real=%.1f.\r\n", cal_pending_cmd, val);
            printf(">> Input next Command Value or 'co' to finish:\r\n");
        }
        return; // 校准模式下不执行后续的普通指令解析
    }

    // --- 2. 普通模式指令解析 (原逻辑) ---

    // [暂停/继续] "p"
    if (strcasecmp(cmd, "p") == 0) {
        if (!ramp.active && !stop_timer.active) {
            printf(">> No active process to pause.\r\n");
            return;
        }
        if (is_paused) {
            is_paused = 0;
            uint32_t paused_duration = HAL_GetTick() - pause_start_tick;
            if (ramp.active) ramp.start_tick += paused_duration;
            if (stop_timer.active) stop_timer.start_tick += paused_duration;
            printf(">> RESUME\r\n");
        } else {
            is_paused = 1;
            pause_start_tick = HAL_GetTick();
            printf(">> PAUSED\r\n");
        }
        return;
    }

    if (is_paused) {
        is_paused = 0; 
        printf(">> New cmd received, pause cancelled.\r\n");
    }

    // 渐变模式 >
    if ((ptr = strchr(cmd, '>')) != NULL) {
        *ptr = '\0'; 
        val1 = atof(cmd);
        char* part2 = ptr + 1;
        char* ptr_t = strchr(part2, 't');
        if (ptr_t) {
            *ptr_t = '\0';
            val2 = atof(part2);
            time_sec = atof(ptr_t + 1);
        } else {
            val2 = atof(part2);
            time_sec = 10.0f;
        }
        ramp.active = 1;
        ramp.start_val = val1;
        ramp.end_val = val2;
        ramp.duration_ms = (uint32_t)(time_sec * 1000);
        ramp.start_tick = HAL_GetTick();
        current_set_speed = val1;
        printf(">> Ramp: %.1f->%.1f (%.1fs)\r\n", val1, val2, time_sec);
    }
    // 快捷增 +
    else if ((ptr = strchr(cmd, '+')) != NULL) {
        *ptr = '\0';
        val1 = atof(cmd);
        time_sec = Parse_Time_Suffix(ptr + 1, 10.0f);
        ramp.active = 1;
        ramp.start_val = val1;
        ramp.end_val = 100.0f;
        ramp.duration_ms = (uint32_t)(time_sec * 1000);
        ramp.start_tick = HAL_GetTick();
        current_set_speed = val1;
        printf(">> Ramp Up: %.1f->100 (%.1fs)\r\n", val1, time_sec);
    }
    // 快捷减 -
    else if ((ptr = strchr(cmd, '-')) != NULL) {
        *ptr = '\0';
        val1 = atof(cmd);
        time_sec = Parse_Time_Suffix(ptr + 1, 10.0f);
        ramp.active = 1;
        ramp.start_val = val1;
        ramp.end_val = 0.0f;
        ramp.duration_ms = (uint32_t)(time_sec * 1000);
        ramp.start_tick = HAL_GetTick();
        current_set_speed = val1;
        printf(">> Ramp Down: %.1f->0 (%.1fs)\r\n", val1, time_sec);
    }
    // 定时 s
    else if ((ptr = strchr(cmd, 's')) != NULL) {
        *ptr = '\0';
        char* part1 = cmd;
        char* part2 = ptr + 1;
        if (strlen(part1) > 0) {
            val1 = atof(part1);
            current_set_speed = val1;
            ramp.active = 0;
        }
        time_sec = atof(part2);
        stop_timer.active = 1;
        stop_timer.delay_ms = (uint32_t)(time_sec * 1000);
        stop_timer.start_tick = HAL_GetTick();
        printf(">> Timer: Run %.1f, Stop in %.1fs\r\n", current_set_speed, time_sec);
    }
    // 普通数值
    else {
        val1 = atof(cmd);
        current_set_speed = val1;
        ramp.active = 0; 
        stop_timer.active = 0;
        printf(">> Set: %.1f\r\n", val1);
    }
}

void Instruction_Loop(void) {
    // 校准模式下不执行自动刷新逻辑，由 Calibration 步骤手动控制输出
    if (sys_mode != MODE_NORMAL) {
        return;
    }

    if (is_paused) return; 

    uint32_t now = HAL_GetTick();

    if (ramp.active) {
        uint32_t elapsed = now - ramp.start_tick;
        if (elapsed >= ramp.duration_ms) {
            current_set_speed = ramp.end_val;
            ramp.active = 0;
            printf(">> Ramp Done.\r\n");
        } else {
            float progress = (float)elapsed / (float)ramp.duration_ms;
            current_set_speed = ramp.start_val + (ramp.end_val - ramp.start_val) * progress;
        }
    }

    if (stop_timer.active) {
        if ((now - stop_timer.start_tick) >= stop_timer.delay_ms) {
            current_set_speed = 0.0f;
            stop_timer.active = 0;
            ramp.active = 0;
            printf(">> Time's up. Stopped.\r\n");
        }
    }

    static uint32_t last_update = 0;
    if (now - last_update >= 20) { 
        Apply_Speed(current_set_speed);
        last_update = now;
    }
}

