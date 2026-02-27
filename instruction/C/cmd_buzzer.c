/**
 * @file cmd_buzzer.c
 * @brief 蜂鸣器控制模块
 * * 职责：
 * 1. 管理 PA3 引脚电平：低电平触发蜂鸣器响，高电平静默。
 * 2. 解析 'buzzer' 或 'b' 指令：切换蜂鸣器开关状态。
 * 3. 解析 'bt[time]' 指令：延时指定秒数后响1秒蜂鸣器。
 * 4. 提供蜂鸣器状态查询和定时控制功能。
 */

#include "cmd_buzzer.h"
#include "instruction_manager.h"
#include "main.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t buzzer_state = 0;         // 0=关闭，1=开启（常开模式）
static uint8_t buzzer_timer_active = 0;  // 1=正在执行定时任务
static uint32_t buzzer_start_time = 0;   // 定时任务开始时间
static uint32_t buzzer_delay_ms = 0;     // 延时时间（毫秒）

void Buzzer_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能GPIOA时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // 配置PA3为推挽输出
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  // 推挽输出
    GPIO_InitStruct.Pull = GPIO_NOPULL;          // 无上下拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // 低频
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 默认设置为高电平（蜂鸣器关闭）
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
    buzzer_state = 0;
    buzzer_timer_active = 0;
}

uint8_t Buzzer_Parse(char* cmd) {
    // 处理基本开关指令
    if (custom_stricmp(cmd, "buzzer") == 0 || custom_stricmp(cmd, "b") == 0) {
        // 取消任何正在进行的定时任务
        buzzer_timer_active = 0;
        
        // 切换蜂鸣器状态
        buzzer_state = !buzzer_state;
        
        if (buzzer_state) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET); // 低电平触发蜂鸣器
            printf(">> Buzzer: ON\r\n");
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);   // 高电平关闭蜂鸣器
            printf(">> Buzzer: OFF\r\n");
        }
        return 1;
    }
    
    // 处理定时蜂鸣指令 bt[time]
    if (strncmp(cmd, "bt", 2) == 0 && strlen(cmd) > 2) {
        char* time_str = cmd + 2;  // 跳过 "bt"
        char* endptr;
        float delay_seconds = strtof(time_str, &endptr);
        
        // 检查转换是否成功
        if (endptr != time_str && *endptr == '\0') {
            buzzer_timer_active = 1;
            buzzer_start_time = HAL_GetTick();
            buzzer_delay_ms = (uint32_t)(delay_seconds * 1000); // 转换为毫秒
            
            printf(">> Buzzer Timer: Will ring after %.1f seconds for 1 second\r\n", delay_seconds);
            return 1;
        }
    }
    
    return 0;
}

void Buzzer_Update(uint32_t now) {
    if (buzzer_timer_active) {
        uint32_t elapsed = now - buzzer_start_time;
        
        // 如果延时期间未到，什么也不做
        if (elapsed < buzzer_delay_ms) {
            return;
        }
        
        // 延时期间已到，开始响1秒
        if (elapsed < buzzer_delay_ms + 1000) {
            // 在响铃期间保持低电平（蜂鸣器响）
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
        } else {
            // 1秒响铃结束，恢复高电平（蜂鸣器静默）
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
            buzzer_timer_active = 0; // 任务完成，取消定时任务
        }
    }
}

void Buzzer_SetState(uint8_t state) {
    // 取消任何正在进行的定时任务
    buzzer_timer_active = 0;
    
    buzzer_state = state;
    if (state) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
    }
}

uint8_t Buzzer_GetState(void) {
    return buzzer_state;
}

uint8_t Buzzer_IsTimerActive(void) {
    return buzzer_timer_active;
}
