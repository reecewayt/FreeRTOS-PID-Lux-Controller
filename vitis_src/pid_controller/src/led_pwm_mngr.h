#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "xil_types.h"
#include "FreeRTOS.h"
// Public function to start the task
void vLedPwm_TaskInit(void);

BaseType_t xLEDPwm_PostDutyCycle(u8 DutyCycle); 
