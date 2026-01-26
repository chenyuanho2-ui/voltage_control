/*
 * instruction.h
 */
#ifndef INC_INSTRUCTION_H_
#define INC_INSTRUCTION_H_

#include <stdint.h>

void Instruction_Init(void);
void Instruction_Parse(char* cmd);
void Instruction_Loop(void);

#endif /* INC_INSTRUCTION_H_ */
