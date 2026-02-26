/**
 * @file cmd_basic.c
 * @brief 基础控制指令模块
 * * 职责：
 * 1. 解析纯数值指令：直接设置目标转速 (例如 "50")。
 * 2. 解析急停指令 's'：立即停止所有渐变、定时任务并将转速归零。
 * 3. 解析暂停指令 'p'：冻结当前的运行状态（渐变或计时）。
 * * 注意：该模块在解析数值指令时会主动调用 Ramp 和 Timer 的 Stop 函数以防止逻辑冲突。
 */

#include "cmd_basic.h"
#include "instruction_manager.h"
#include "cmd_ramp.h"   
#include "cmd_timer.h"  
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


uint8_t Basic_Parse(char* cmd) {
    // 紧急停止 s
    if (custom_stricmp(cmd, "s") == 0) {
        Ramp_Stop();
        Timer_Stop();
        Instruction_SetSpeed(0.0f);
        return 1;
    }
    // 暂停/继续 p
    if (strcasecmp(cmd, "p") == 0) {
        // ... 原 p 指令逻辑 ...
        return 1;
    }
    // 纯数值判断
    if (isdigit(cmd[0])) {
        Instruction_SetSpeed(atof(cmd));
        Ramp_Stop();
        Timer_Stop();
        return 1;
    }
    return 0;
}
