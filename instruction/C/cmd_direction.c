/**
 * @file cmd_direction.c
 * @brief 旋转方向控制模块
 * * 职责：
 * 1. 管理 PB12 引脚电平：低电平为顺时针 (CW)，高电平为逆时针 (CCW)。
 * 2. 解析 'ch0'：设置为顺时针。
 * 3. 解析 'ch1'：设置为逆时针。
 * 4. 解析 'ch' ：切换当前转向。
 */

#include "cmd_direction.h"
#include "instruction_manager.h"
#include "main.h"
#include "gpio.h"
#include <stdio.h>

void Direction_Init(void) {
    // 默认设置为低电平（顺时针）
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
}

uint8_t Direction_Parse(char* cmd) {
    if (custom_stricmp(cmd, "CW") == 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
        printf(">> Direction: Clockwise (CW)\r\n");
        return 1;
    }
    else if (custom_stricmp(cmd, "CCW") == 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
        printf(">> Direction: Counter-Clockwise (CCW)\r\n");
        return 1;
    }
    else if (custom_stricmp(cmd, "change") == 0) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
        GPIO_PinState state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12);
        printf(">> Direction Toggled: %s\r\n", (state == GPIO_PIN_SET) ? "CCW" : "CW");
        return 1;
    }
    return 0;
}
