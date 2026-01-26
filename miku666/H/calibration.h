/*
 * calibration.h
 *
 * 用于MCP4725 DAC控制蠕动泵的转速校准
 */

#ifndef CALIBRATION_H_
#define CALIBRATION_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  根据目标转速，计算需要发送给DAC的修正后的指令值
 * @param  target_speed: 期望的实际转速 (0-100)
 * @return float: 修正后的输入指令值 (0-100)，用于计算DAC电压
 */
float Calibration_GetCorrectedValue(float target_speed);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_H_ */
