/**
 * @file log_mngr.c
 * @brief Data Logging Manager implementation
 * 
 * This module:
 * 1. logs PID control system state in CSV format for analysis.
 * 2. logs setpoint, and measured lux to observable 7-segment display.
 * 
 * @note Only active when debug logging is disabled to avoid cluttering output.
 */
#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include "config.h"

/**
 * @brief Control system state snapshot for logging
 */
typedef struct {
    uint32_t timestamp_ms;   /**< Timestamp in milliseconds */
    uint32_t lux_setpoint;   /**< Target lux value */
    uint32_t lux_measured;   /**< Actual measured lux value */
    uint32_t kp;             /**< Proportional gain (scaled) */
    uint32_t ki;             /**< Integral gain (scaled) */
    uint32_t kd;             /**< Derivative gain (scaled) */
    int32_t p_term;          /**< Proportional term contribution */
    int32_t i_term;          /**< Integral term contribution */
    int32_t d_term;          /**< Derivative term contribution */
    uint8_t pid_output;      /**< PWM output (0-255) */
} LogData_t;

/**
 * @brief Initialize and create the Log Manager task
 * 
 * This function creates a FreeRTOS task that will:
 * 1. Wait for log data from the PID compute task
 * 2. Format and print data in CSV format
 * 3. Optionally implement data buffering/filtering
 * 
 * The task begins running immediately after creation.
 * 
 * @return pdPASS if initialization successful, pdFAIL otherwise
 */
BaseType_t vLogMngr_Init(void);

/**
 * @brief Post log data to the logging task
 * 
 * This function should be called by the PID compute task after each
 * control loop iteration to log the system state.
 * 
 * @param logData Pointer to log data structure
 * @return pdPASS if posted successfully, pdFAIL otherwise
 */
BaseType_t xLogMngr_Post(const LogData_t *logData);

/**
 * @brief Post log data from ISR context
 * 
 * @param logData Pointer to log data structure
 * @param pxHigherPriorityTaskWoken Pointer for context switch flag
 * @return pdPASS if posted successfully, pdFAIL otherwise
 */
BaseType_t xLogMngr_PostFromISR(const LogData_t *logData, BaseType_t *pxHigherPriorityTaskWoken);
