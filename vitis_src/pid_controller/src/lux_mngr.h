/**
 * @file lux_mngr.h
 * @brief Lux Manager - Periodic light sensor reading task
 * 
 * This module implements a FreeRTOS task that periodically reads the TSL2561
 * light sensor and posts lux measurements to the PID controller. The task
 * uses vTaskDelay() to implement the sample period (dt).
 */

#pragma once 

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Initialize and create the Lux Manager task
 * 
 * This function creates a FreeRTOS task that will:
 * 1. Initialize the TSL2561 sensor
 * 2. Periodically read both channels at the specified sample rate
 * 3. Calculate lux values
 * 4. Post measurements to the PID controller
 * 
 * The task begins running immediately after creation.
 * 
 * @param samplePeriodMs Sample period in milliseconds (dt for PID controller)
 * @return pdPASS if initialization successful, pdFAIL otherwise
 */
BaseType_t vLuxMngr_Init(uint32_t samplePeriodMs);

