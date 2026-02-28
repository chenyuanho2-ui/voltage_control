#include "cmd_self_test.h"
#include "instruction_manager.h"
#include "cmd_buzzer.h"
#include "adc.h"
#include "i2c.h"
#include <stdio.h>
#include <string.h>

// 引用 main.c 中的 DAC 输出函数
extern void MCP4725_SetVoltage(uint16_t output, uint8_t writeEEPROM);

// 辅助函数：获取 ADC 电压均值 (1s 采样)
static float Get_Average_Voltage(void) {
    float sum = 0;
    for (int i = 0; i < 50; i++) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            uint32_t val = HAL_ADC_GetValue(&hadc1);
            sum += (float)val * 3.3f / 4095.0f; // 假设 3.3V 参考电压
        }
        HAL_Delay(20); // 20ms * 50 = 1000ms
    }
    return sum / 50.0f;
}

int SelfTest_Parse(char* cmd) {
    if (strcasecmp(cmd, "st") == 0 || strcasecmp(cmd, "selfcheck") == 0) {
        SelfTest_Run();
        return 1;
    }
    return 0;
}

void SelfTest_Run(void) {
    float test_points[] = {20.0f, 60.0f, 100.0f};
    float results[3];
    
    // 强制切换到自检模式，挂起 Normal 逻辑
    Instruction_SetMode(MODE_SELFCHECK);
    printf("\r\n--- SYSTEM SELF-CHECK START ---\r\n");

    // 1. 蜂鸣器响应 1s
    Buzzer_SetState(1);
    HAL_Delay(1000);
    Buzzer_SetState(0);

    // 2. 依次测试三个点位 (20%, 60%, 100%)
    for (int i = 0; i < 3; i++) {
        uint16_t dac_val = (uint16_t)(test_points[i] * 40.95f);
        MCP4725_SetVoltage(dac_val, 0); // 输出原始电压
        
        printf("Testing Speed %0.f%%... ", test_points[i]);
        
        // A. 等待硬件稳定 (0.5s)
        HAL_Delay(500);
        
        // B. 采样均值 (1s)
        results[i] = Get_Average_Voltage();
        printf("ADC: %.2fV\r\n", results[i]);
    }

    // 3. 线性度判断
    float diff1 = results[1] - results[0]; // 20% -> 60%
    float diff2 = results[2] - results[1]; // 60% -> 100%
    
    printf("\r\n--- SELF-CHECK REPORT ---\r\n");
    if (results[2] < 0.1f) {
        printf("[FAIL] No voltage detected. Check Hardware!\r\n");
    } else {
        float ratio = (diff1 > diff2) ? (diff1 / diff2) : (diff2 / diff1);
        if (ratio < 1.2f) { // 允许 20% 偏差
            printf("[PASS] Linearity OK (Ratio: %.2f)\r\n", ratio);
        } else {
            printf("[WARN] Linearity poor (Ratio: %.2f). Calibration suggested.\r\n", ratio);
        }
    }
    
    // 恢复正常
    MCP4725_SetVoltage(0, 0);
    Instruction_SetMode(MODE_NORMAL);
    printf("--- SELF-CHECK FINISHED ---\r\n\r\n");
}


