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
 * ==============================================================================
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
static float current_set_speed = 0.0f; // 当前逻辑设定的目标速度

// 状态标志
static uint8_t is_paused = 0;           // 暂停标志
static uint32_t pause_start_tick = 0;   // 暂停开始的时间戳

// 渐变控制结构体
typedef struct {
    uint8_t active;       // 是否正在渐变
    float start_val;      // 起始值
    float end_val;        // 终点值
    uint32_t start_tick;  // 开始时间戳
    uint32_t duration_ms; // 持续时间
} Ramp_t;

// 定时停止结构体
typedef struct {
    uint8_t active;       // 是否开启定时
    uint32_t start_tick;  // 开始计时的时间戳
    uint32_t delay_ms;    // 延时时长
} Timer_t;

static Ramp_t ramp = {0};
static Timer_t stop_timer = {0};

// --- 内部函数：执行最终的硬件设置 ---
static void Apply_Speed(float speed) {
    if (speed < 0.0f) speed = 0.0f;
    if (speed > 100.0f) speed = 100.0f;

    float corrected = Calibration_GetCorrectedValue(speed);
    uint16_t dac_val = (uint16_t)(corrected * 40.95f);
    if (dac_val > 4095) dac_val = 4095;

    MCP4725_SetVoltage(dac_val, 0);
}

void Instruction_Init(void) {
    current_set_speed = 0.0f;
    ramp.active = 0;
    stop_timer.active = 0;
    is_paused = 0;
    Apply_Speed(0.0f);
}

// 辅助函数：解析时间参数 (例如 "t5" 返回 5.0，空字符串返回默认值)
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

    // --- 0. 优先处理特殊单字符命令 ---
    
    // [急停] "s" 或 "S" (精确匹配)
    // 必须放在 "s" 定时指令解析之前
    if (strcasecmp(cmd, "s") == 0) {
        is_paused = 0;
        ramp.active = 0;
        stop_timer.active = 0;
        current_set_speed = 0.0f;
        Apply_Speed(0.0f); // 立即归零
        printf(">> STOP (0)\r\n");
        return;
    }

    // [暂停/继续] "p" 或 "P"
    if (strcasecmp(cmd, "p") == 0) {
        if (!ramp.active && !stop_timer.active) {
            printf(">> No active process to pause.\r\n");
            return;
        }

        if (is_paused) {
            // == 恢复 (Resume) ==
            is_paused = 0;
            // 计算暂停了多久
            uint32_t paused_duration = HAL_GetTick() - pause_start_tick;
            
            // 补偿开始时间，让进度条"接上"
            if (ramp.active) ramp.start_tick += paused_duration;
            if (stop_timer.active) stop_timer.start_tick += paused_duration;
            
            printf(">> RESUME\r\n");
        } else {
            // == 暂停 (Pause) ==
            is_paused = 1;
            pause_start_tick = HAL_GetTick();
            printf(">> PAUSED\r\n");
        }
        return;
    }

    // 如果输入其他指令，默认取消暂停状态，执行新指令
    if (is_paused) {
        is_paused = 0; 
        printf(">> New cmd received, pause cancelled.\r\n");
    }

    // --- 1. 渐变模式 (包含 '>') ---
    if ((ptr = strchr(cmd, '>')) != NULL) {
        *ptr = '\0'; 
        val1 = atof(cmd);           // 起始
        char* part2 = ptr + 1;
        
        // 查找 part2 里有没有 't'
        char* ptr_t = strchr(part2, 't');
        if (ptr_t) {
            *ptr_t = '\0';
            val2 = atof(part2);     // 终点
            time_sec = atof(ptr_t + 1); // 时间
        } else {
            val2 = atof(part2);
            time_sec = 10.0f;       // 默认10s
        }

        ramp.active = 1;
        ramp.start_val = val1;
        ramp.end_val = val2;
        ramp.duration_ms = (uint32_t)(time_sec * 1000);
        ramp.start_tick = HAL_GetTick();
        
        current_set_speed = val1;
        printf(">> Ramp: %.1f->%.1f (%.1fs)\r\n", val1, val2, time_sec);
    }
    // --- 2. 快捷指令 ('+') 50+ / 50+t5 ---
    else if ((ptr = strchr(cmd, '+')) != NULL) {
        *ptr = '\0';
        val1 = atof(cmd);
        
        // 解析 + 后面的内容
        time_sec = Parse_Time_Suffix(ptr + 1, 10.0f); // 默认10s
        
        ramp.active = 1;
        ramp.start_val = val1;
        ramp.end_val = 100.0f;
        ramp.duration_ms = (uint32_t)(time_sec * 1000);
        ramp.start_tick = HAL_GetTick();
        
        current_set_speed = val1;
        printf(">> Ramp Up: %.1f->100 (%.1fs)\r\n", val1, time_sec);
    }
    // --- 3. 快捷指令 ('-') 50- / 50-t5 ---
    else if ((ptr = strchr(cmd, '-')) != NULL) {
        *ptr = '\0';
        val1 = atof(cmd);

        // 解析 - 后面的内容
        time_sec = Parse_Time_Suffix(ptr + 1, 10.0f); // 默认10s
        
        ramp.active = 1;
        ramp.start_val = val1;
        ramp.end_val = 0.0f;
        ramp.duration_ms = (uint32_t)(time_sec * 1000);
        ramp.start_tick = HAL_GetTick();
        
        current_set_speed = val1;
        printf(">> Ramp Down: %.1f->0 (%.1fs)\r\n", val1, time_sec);
    }
    // --- 4. 定时指令 ('s') 50s15 / s15 ---
    // 注意：这里检查的是作为分隔符的 's'，上面已经排除了单独的 "s" 命令
    else if ((ptr = strchr(cmd, 's')) != NULL) {
        *ptr = '\0';
        char* part1 = cmd;
        char* part2 = ptr + 1;

        if (strlen(part1) > 0) {
            val1 = atof(part1);
            current_set_speed = val1;
            ramp.active = 0; // 直接设置速度，取消渐变
        }
        
        time_sec = atof(part2);
        stop_timer.active = 1;
        stop_timer.delay_ms = (uint32_t)(time_sec * 1000);
        stop_timer.start_tick = HAL_GetTick();

        printf(">> Timer: Run %.1f, Stop in %.1fs\r\n", current_set_speed, time_sec);
    }
    // --- 5. 普通数值 ---
    else {
        val1 = atof(cmd);
        current_set_speed = val1;
        ramp.active = 0; 
        stop_timer.active = 0;
        printf(">> Set: %.1f\r\n", val1);
    }
}

void Instruction_Loop(void) {
    // 如果处于暂停状态，什么都不做，直接返回
    // 保持 current_set_speed 不变，硬件输出维持在暂停时的电压
    if (is_paused) {
        return; 
    }

    uint32_t now = HAL_GetTick();

    // --- 处理渐变逻辑 ---
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

    // --- 处理定时停止逻辑 ---
    if (stop_timer.active) {
        if ((now - stop_timer.start_tick) >= stop_timer.delay_ms) {
            current_set_speed = 0.0f;
            stop_timer.active = 0;
            ramp.active = 0;
            printf(">> Time's up. Stopped.\r\n");
        }
    }

    // --- 刷新硬件输出 ---
    // 简单限频，防止I2C过载
    static uint32_t last_update = 0;
    if (now - last_update >= 20) { 
        Apply_Speed(current_set_speed);
        last_update = now;
    }
}
