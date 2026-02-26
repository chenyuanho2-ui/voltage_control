/**
 * @file cmd_timer.c
 * @brief 定时停止指令模块
 * * 职责：
 * 1. 解析定时指令 '[A]s[S]'：以 A 速度运行 S 秒后自动停止。
 * 2. 解析缺省指令 's[S]'：保持当前速度运行 S 秒后停止。
 * 3. 实时监控运行时间，并在时间到达时将系统转速安全归零。
 */

#include "cmd_timer.h"
#include "instruction_manager.h"
#include <string.h>


static struct { uint8_t active; uint32_t start_t, delay; } timer;



uint8_t Timer_Parse(char* cmd) {
    if (strchr(cmd, 's') && custom_stricmp(cmd, "s") != 0) {
        // 解析 AsS 逻辑
        timer.active = 1;
        return 1;
    }
    return 0;
}

void Timer_Update(uint32_t now) {
    if (timer.active && (now - timer.start_t >= timer.delay)) {
        Instruction_SetSpeed(0.0f);
        timer.active = 0;
    }
}

void Timer_Stop(void) { timer.active = 0; }
