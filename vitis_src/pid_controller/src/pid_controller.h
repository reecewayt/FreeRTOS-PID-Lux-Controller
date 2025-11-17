#pragma once

#include "xil_types.h"
#include <stdbool.h>
#include <stdint.h>

/************************** Error Codes ***************************************/
#define PID_SUCCESS             0
#define PID_ERROR_NULL_PTR      -1
#define PID_ERROR_NOT_INIT      -2
#define PID_ERROR_INVALID_PARAM -3

/* Gain scaling factor - gains are multiplied by this for precision */
#define PID_GAIN_SCALE          1000

/* Default settings */ 
#define PID_DEFAULT_OUTPUT_MAX  255
#define PID_DEFAULT_OUTPUT_MIN  0
#define PID_INTEGRAL_MAX        100000  /* Scaled integral limit */
#define PID_INTEGRAL_MIN        -100000 /* Scaled integral limit */
#define PID_DEFAULT_SAMPLE_TIME_MS 100

/* Control term enable flags */
#define PID_ENABLE_P            (1 << 0)  /* Bit 0: Proportional */
#define PID_ENABLE_I            (1 << 1)  /* Bit 1: Integral */
#define PID_ENABLE_D            (1 << 2)  /* Bit 2: Derivative */
#define PID_ENABLE_ALL          (PID_ENABLE_P | PID_ENABLE_I | PID_ENABLE_D)

typedef enum {
    LOW_INCR = 1,
    MED_INCR = 5,
    HIGH_INCR = 10
} step_incr_t;

/**
 * @brief PID Controller Configuration Structure
 * 
 * Note: Gains (kp, ki, kd) are scaled by PID_GAIN_SCALE (1000) for precision.
 * For example, to set Kp = 1.5, use kp = 1500
 */
typedef struct {
    uint32_t kp;             /**< Proportional gain (Kp * PID_GAIN_SCALE) */
    uint32_t ki;             /**< Integral gain (Ki * PID_GAIN_SCALE) */
    uint32_t kd;             /**< Derivative gain (Kd * PID_GAIN_SCALE) */
    
    int32_t output_min;     /**< Minimum output value (saturation) 0-255 */
    int32_t output_max;     /**< Maximum output value (saturation) 0-255 */
    
    int32_t integral_max;   /**< Maximum integral term (anti-windup, scaled) */
    int32_t integral_min;   /**< Minimum integral term (anti-windup, scaled) */
    
    uint32_t setpoint;      /**< Setpoint for controller (lux value) */ 
    uint32_t max_setpoint;  /**< Maximum setpoint that makes sense for plant */
    
    uint8_t enable_flags;   /**< Enable/disable P, I, D terms (bit flags) */
    step_incr_t step;       /**< Step increment for constant terms (i.e. +/- 1, 5, and 10) */
    
    uint32_t sample_time_ms; /**< Sample time in milliseconds (for Ki, Kd scaling) */
} PID_Config;

/**
 * @brief PID Controller State Structure
 */
typedef struct {
    /* Configuration */
    PID_Config config;
    
    /* State variables */
    int32_t integral;       /**< Accumulated integral term (scaled) */
    int32_t prev_error;     /**< Previous error (for derivative) */
    uint32_t prev_measurement; /**< Previous measurement (for derivative-on-measurement) */
    
    /* Output terms (for debugging/monitoring) */
    int32_t p_term;         /**< Last proportional term (scaled) */
    int32_t i_term;         /**< Last integral term (scaled) */
    int32_t d_term;         /**< Last derivative term (scaled) */
    uint8_t output;         /**< Last controller output (0-255 for PWM) */
    
    /* Flags */
    bool initialized;       /**< Initialization flag */
} PID_Controller;

/************************** Function Prototypes *******************************/

/**
 * @brief Initialize PID controller with default configuration
 * 
 * @param pid Pointer to PID controller structure
 * @return 0 on success, negative error code on failure
 */
int32_t PID_Init(PID_Controller *pid);

/**
 * @brief Configure PID controller with custom parameters
 * 
 * @param pid Pointer to PID controller structure
 * @param config Pointer to configuration structure
 * @return 0 on success, negative error code on failure
 */
int32_t PID_Configure(PID_Controller *pid, const PID_Config *config);

/**
 * @brief Enable/disable individual PID control terms
 * 
 * @param pid Pointer to PID controller structure
 * @param enable_flags Bit flags: PID_ENABLE_P, PID_ENABLE_I, PID_ENABLE_D
 * @return 0 on success, negative error code on failure
 * 
 * Example:
 *   PID_SetEnableFlags(pid, PID_ENABLE_P | PID_ENABLE_I);  // Enable P and I only
 * 
 * Note: Disabling integral term will clear accumulated integral to prevent windup
 */
int32_t PID_SetEnableFlags(PID_Controller *pid, uint8_t enable_flags);

/**
 * @brief Compute PID controller output
 * 
 * This is the main PID calculation function. Call this at each control loop iteration.
 * 
 * @param pid Pointer to PID controller structure
 * @param measurement Current measured lux value (uint32_t)
 * @return Controller output (0-255 for PWM duty cycle)
 * 
 * Note: The function automatically handles:
 *   - Error calculation (setpoint - measurement)
 *   - Integral accumulation with anti-windup
 *   - Derivative calculation
 *   - Output saturation
 *   - Enable/disable flags for each term
 */
uint8_t PID_Compute(PID_Controller *pid, uint32_t measurement);

/**
 * @brief Reset PID controller state (clear integral, previous error)
 * 
 * Call this when changing setpoint significantly or when starting control
 * 
 * @param pid Pointer to PID controller structure
 * @return 0 on success, negative error code on failure
 */
int32_t PID_Reset(PID_Controller *pid);




