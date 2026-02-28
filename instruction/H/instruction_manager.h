#ifndef INSTRUCTION_MANAGER_H_
#define INSTRUCTION_MANAGER_H_

#include <stdint.h>

// 定义系统运行模式
typedef enum {
    MODE_NORMAL = 0,
    MODE_CALIBRATION,
	MODE_SELFCHECK
} SystemMode_t;

void Instruction_Init(void);
void Instruction_Parse(char* cmd);
void Instruction_Loop(void);

// 供子模块使用的公共 API
void Instruction_SetSpeed(float speed);
float Instruction_GetSpeed(void);
void Instruction_SetMode(SystemMode_t mode);
SystemMode_t Instruction_GetMode(void);
// 在 instruction_manager.h 中添加
int custom_stricmp(const char *s1, const char *s2);

#endif
