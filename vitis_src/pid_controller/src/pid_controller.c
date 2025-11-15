/**
 * @file pid_controller.c
 * @brief Integer-based PID Controller Implementation
 * 
 * This module implements a PID controller using integer arithmetic with scaled gains.
 * The implementation includes:
 * - Standard PID algorithm with configurable gains (scaled by PID_GAIN_SCALE)
 * - Integral anti-windup using clamping
 * - Output saturation (0-255 for PWM duty cycle)
 * - Individual enable/disable for P, I, D terms
 * - Standard derivative calculation
 * 
 * Input: uint32_t lux measurement from TLS2561 sensor
 * Output: uint8_t PWM duty cycle (0-255)
 * 
 * @author ECE 544 Team
 * @date 2025
 */

/************************** Include Files *************************************/
#include "pid_controller.h"
#include <string.h>

/************************** Private Functions **********************************/
static inline int32_t saturate_int32(int32_t value, int32_t min, int32_t max) {
    if(value > max){
        return max;
    }

    if(value < min) {
        return min;
    }

    return value;     
}

/************************** Public Functions **********************************/

/**
 * @brief Initialize PID controller with default configuration
 */
int32_t PID_Init(PID_Controller *pid) {
    if (pid == NULL) {
        return PID_ERROR_NULL_PTR;
    }
    
    /* Clear the entire structure */
    memset(pid, 0, sizeof(PID_Controller));
    
    /* Set default configuration */
    pid->config.kp = 1 * PID_GAIN_SCALE;  /* Kp = 1.0 */
    pid->config.ki = 0;                    /* Ki = 0.0 */
    pid->config.kd = 0;                    /* Kd = 0.0 */
    
    pid->config.output_min = PID_DEFAULT_OUTPUT_MIN;
    pid->config.output_max = PID_DEFAULT_OUTPUT_MAX;
    
    pid->config.integral_min = PID_INTEGRAL_MIN;
    pid->config.integral_max = PID_INTEGRAL_MAX;
    
    pid->config.setpoint = 0;
    pid->config.max_setpoint = 65535;  /* Reasonable default for lux sensor */
    
    pid->config.enable_flags = PID_ENABLE_ALL;
    pid->config.sample_time_ms = PID_DEFAULT_SAMPLE_TIME_MS;
    pid->config.step = LOW_INCR;
    
    /* Initialize state */
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = 0;
    
    pid->p_term = 0;
    pid->i_term = 0;
    pid->d_term = 0;
    pid->output = 0;
    
    pid->initialized = true;
    
    return PID_SUCCESS;
}

/**
 * @brief Configure PID controller with custom parameters
 */
int32_t PID_Configure(PID_Controller *pid, const PID_Config *config) {
    if (pid == NULL || config == NULL) {
        return PID_ERROR_NULL_PTR;
    }
    
    /* Copy configuration */
    memcpy(&pid->config, config, sizeof(PID_Config));
    
    /* Reset state when reconfiguring */
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = 0;
    
    pid->initialized = true;
    
    return PID_SUCCESS;
}

/**
 * @brief Enable/disable individual PID control terms
 */
int32_t PID_SetEnableFlags(PID_Controller *pid, uint8_t enable_flags) {
    if (pid == NULL) {
        return PID_ERROR_NULL_PTR;
    }
    
    if (!pid->initialized) {
        return PID_ERROR_NOT_INIT;
    }
    
    pid->config.enable_flags = enable_flags & PID_ENABLE_ALL;
    
    /* Reset integral when I term is disabled to prevent windup */
    if (!(enable_flags & PID_ENABLE_I)) {
        pid->integral = 0;
        pid->i_term = 0;
    }
    
    return PID_SUCCESS;
}

/**
 * @brief Compute PID controller output
 * 
 * Uses integer arithmetic with scaled gains for precision.
 * Input: uint32_t lux measurement
 * Output: uint8_t PWM duty cycle (0-255)
 */
uint8_t PID_Compute(PID_Controller *pid, uint32_t measurement) {
    if (pid == NULL || !pid->initialized) {
        return 0;
    }
    
    /* Calculate error (signed, as setpoint might be less than measurement) */
    int32_t error = (int32_t)pid->config.setpoint - (int32_t)measurement;
    
    /* Initialize terms to zero */
    pid->p_term = 0;
    pid->i_term = 0;
    pid->d_term = 0;
    
    /* ========== Proportional Term ========== */
    /* P_term = Kp * error (Kp is scaled, so divide by scale factor) */
    if (pid->config.enable_flags & PID_ENABLE_P) {
        pid->p_term = (pid->config.kp * error) / PID_GAIN_SCALE;
    }
    
    /* ========== Integral Term ========== */
    if (pid->config.enable_flags & PID_ENABLE_I) {
        /* Accumulate error for integral */
        pid->integral += error;
        
        /* Apply anti-windup (clamp integral) */
        pid->integral = saturate_int32(pid->integral, 
                                       pid->config.integral_min, 
                                       pid->config.integral_max);
        
        /* Calculate integral term: I_term = Ki * integral */
        pid->i_term = (pid->config.ki * pid->integral) / PID_GAIN_SCALE;
    }
    
    /* ========== Derivative Term ========== */
    if (pid->config.enable_flags & PID_ENABLE_D) {
        /* D_term = Kd * (error - prev_error) */
        int32_t derivative = error - pid->prev_error;
        pid->d_term = (pid->config.kd * derivative) / PID_GAIN_SCALE;
    }
    
    /* ========== Calculate Total Output ========== */
    int32_t output_raw = pid->p_term + pid->i_term + pid->d_term;
    
    /* Apply output saturation (0-255 for PWM) */
    output_raw = saturate_int32(output_raw, 
                                pid->config.output_min, 
                                pid->config.output_max);
    
    /* Cast to uint8_t for PWM output */
    pid->output = (uint8_t)output_raw;
    
    /* Store values for next iteration */
    pid->prev_error = error;
    pid->prev_measurement = measurement;
    
    return pid->output;
}

/**
 * @brief Reset PID controller state
 */
int32_t PID_Reset(PID_Controller *pid) {
    if (pid == NULL) {
        return PID_ERROR_NULL_PTR;
    }
    
    if (!pid->initialized) {
        return PID_ERROR_NOT_INIT;
    }
    
    /* Clear state variables */
    pid->integral = 0;
    pid->prev_error = 0;
    pid->prev_measurement = 0;
    
    pid->p_term = 0;
    pid->i_term = 0;
    pid->d_term = 0;
    pid->output = 0;
    
    return PID_SUCCESS;
}


