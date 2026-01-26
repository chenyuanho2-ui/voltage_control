/*
 * calibration.c
 *
 * 实现分段线性插值算法，用于校正非线性误差
 */

#include "calibration.h"

// 校准点的数量（根据你提供的数据，有10组）
#define CAL_POINTS 10

/* * X轴：实际测量到的转速 (Measured Speed)
 * 这是你希望达到的真实物理效果
 */
static const float measured_speeds[CAL_POINTS] = {
    0.0f, 10.0f, 20.3f, 30.7f, 41.2f, 
    51.5f, 61.9f, 72.1f, 82.4f, 92.8f
};

/* * Y轴：对应的输入指令值 (Command Input)
 * 这是当时为了达到上述转速，你实际在串口输入的数值 (0-90)
 */
static const float command_inputs[CAL_POINTS] = {
    0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 
    50.0f, 60.0f, 70.0f, 80.0f, 90.0f
};

/**
 * @brief  输入你想要的目标转速，返回应该设置的指令值
 */
float Calibration_GetCorrectedValue(float target_speed) {
    int i;

    // 1. 处理下限：如果目标小于0，返回0
    if (target_speed <= measured_speeds[0]) {
        return command_inputs[0];
    }

    // 2. 处理上限：如果目标超过了我们测量的最大值(92.8)
    // 使用最后一段的斜率进行线性外推 (Extrapolation)
    if (target_speed >= measured_speeds[CAL_POINTS - 1]) {
        float x1 = measured_speeds[CAL_POINTS - 2];
        float x2 = measured_speeds[CAL_POINTS - 1];
        float y1 = command_inputs[CAL_POINTS - 2];
        float y2 = command_inputs[CAL_POINTS - 1];
        
        float slope = (y2 - y1) / (x2 - x1);
        return y2 + slope * (target_speed - x2);
    }

    // 3. 区间查找与线性插值 (Interpolation)
    // 遍历查找 target_speed 落在哪两个校准点之间
    for (i = 0; i < CAL_POINTS - 1; i++) {
        if (target_speed >= measured_speeds[i] && target_speed < measured_speeds[i+1]) {
            float x1 = measured_speeds[i];
            float x2 = measured_speeds[i+1];
            float y1 = command_inputs[i];
            float y2 = command_inputs[i+1];

            // 线性插值公式: y = y1 + (x - x1) * (y2 - y1) / (x2 - x1)
            float ratio = (target_speed - x1) / (x2 - x1);
            return y1 + ratio * (y2 - y1);
        }
    }

    return target_speed; // 理论上不会运行到这里
}
