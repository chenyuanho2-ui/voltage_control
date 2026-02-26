/**
 * @file cmd_ramp.c
 * @brief 线性渐变指令模块
 * * 职责：
 * 1. 解析渐变指令 '[A]>[B][tS]'：在 S 秒内从 A% 匀速变化到 B%。
 * 2. 处理快捷指令 '+' (升至100%) 和 '-' (降至0%)。
 * 3. 基于时间步进计算每一时刻的插值速度，确保电机运行平稳。
 */

#include "cmd_ramp.h"
#include "instruction_manager.h"
#include <string.h>
#include <stdlib.h>

static struct { uint8_t active; float start, end; uint32_t start_t, dur; } ramp;

uint8_t Ramp_Parse(char* cmd) {
    if (strchr(cmd, '>') != NULL) {
        // 解析 A>B tS 逻辑
        // ... 解析代码 ...
        ramp.active = 1;
        return 1;
    }
    // 处理 + 和 - 快捷指令
    if (strchr(cmd, '+') || strchr(cmd, '-')) {
        // ... 解析代码 ...
        ramp.active = 1;
        return 1;
    }
    return 0;
}

void Ramp_Update(uint32_t now) {
    if (!ramp.active) return;
    // ... 原渐变数学逻辑 ...
    // Instruction_SetSpeed(计算结果);
}

void Ramp_Stop(void) { ramp.active = 0; }
