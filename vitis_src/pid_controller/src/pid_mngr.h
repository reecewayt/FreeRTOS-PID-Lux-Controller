#pragma once

#include "xil_types.h"
#include "pid_controller.h"
#include "FreeRTOS.h"
#include "queue.h"

//********************* Type Definitions *****************************//

/**
 * @brief User input state structure
 * Contains current switch and button states for PID configuration
 */
typedef struct {
    uint8_t sw;     /**< Switch state (8 bits) */
    uint8_t btn;    /**< Button state (5 bits: C, U, D, L, R) */
} UserInputState_t;


//******************** RTOS Interface to PID Controller **************//

/**
 * @brief Initialize and create PID controller tasks
 * 
 * This function initializes the PID controller and creates two FreeRTOS tasks:
 * 1. User Input Task - Processes switch/button inputs for PID configuration
 * 2. PID Compute Task - Processes lux measurements and computes control output
 * 
 * Must be called before using any other PID manager functions.
 */
void vPID_TaskCreate(void);

/**
 * @brief Post user input state to the User Input Task
 * 
 * This function should be called from a task to send new
 * switch and button states for processing.
 * 
 * @param pxNewState Current switch and button state
 * @return pdPASS if posted successfully, pdFAIL otherwise
 */
BaseType_t xUserInput_Post(UserInputState_t pxNewState);

/**
 * @brief Post user input state to the User Input Task from ISR
 * 
 * This function should be called from an ISR to send new
 * switch and button states for processing.
 * 
 * @param pxNewState Current switch and button state
 * @param pxHigherPriorityTaskWoken Pointer to variable that will be set to pdTRUE if context switch is needed
 * @return pdPASS if posted successfully, pdFAIL otherwise
 */
BaseType_t xUserInput_PostFromISR(UserInputState_t pxNewState, BaseType_t *pxHigherPriorityTaskWoken);

/**
 * @brief Post lux measurement to the PID Compute Task
 * 
 * This function should be called when a new lux measurement is available
 * from the TLS2561 sensor. The PID controller will compute a new PWM
 * duty cycle based on this measurement.
 * 
 * @param luxMeasurement Current lux reading from sensor
 * @return pdPASS if posted successfully, pdFAIL otherwise
 */
BaseType_t xPIDCompute_PostMeasurement(uint32_t luxMeasurement);


