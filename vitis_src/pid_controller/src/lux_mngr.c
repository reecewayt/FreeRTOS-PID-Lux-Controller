/**
 * @file lux_mngr.c
 * @brief Lux Manager implementation - Periodic TSL2561 sensor reading
 * 
 * This module implements a simple periodic task that reads the TSL2561
 * light sensor at regular intervals and posts measurements to the PID controller.
 */

#include "lux_mngr.h"
#include "pid_mngr.h"
#include "tsl2561.h"
#include "FreeRTOS.h"
#include "task.h"
#include "xiic.h"

#define LOG_TAG "LUX_MNGR"
#include "logging.h"

//********************* Configuration *****************************//

#define LUX_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 2)
#define LUX_TASK_PRIORITY       (tskIDLE_PRIORITY + 3)

//********************* Private Variables *************************//

static XIic xI2CInstance;
static uint32_t ulSamplePeriodMs = 0;

//********************* Lux Manager Task **************************//

/**
 * @brief Lux Manager task implementation
 * 
 * This task runs in a loop with the configured sample period:
 * 1. Reads both TSL2561 channels (CH0 and CH1)
 * 2. Calculates the lux value
 * 3. Posts the measurement to the PID controller
 * 4. Delays for the sample period
 * 
 * @param pvParameters Task parameters (unused)
 */
static void prvLuxMngr_Task(void *pvParameters)
{
    (void)pvParameters;
    
    uint16_t ch0, ch1;
    uint32_t lux;
    BaseType_t xStatus;
    TickType_t xDelay;
    
    DEBUG_PRINT("Lux Manager task started (sample period: %lu ms)\n", ulSamplePeriodMs);
    
    // Initialize the TSL2561 sensor
    tsl2561_init(&xI2CInstance);
    DEBUG_PRINT("TSL2561 sensor initialized\n");
    
    // Convert sample period to ticks
    xDelay = pdMS_TO_TICKS(ulSamplePeriodMs);
    
    for (;;)
    {
        // Read channel 0 (broadband - visible + infrared)
        ch0 = tsl2561_readChannel(&xI2CInstance, TSL2561_CHANNEL_0);
        
        // Read channel 1 (infrared only)
        ch1 = tsl2561_readChannel(&xI2CInstance, TSL2561_CHANNEL_1);
        
        // Calculate lux value
        lux = tsl2561_calculateLux(ch0, ch1);
        
        // Post measurement to PID controller
        xStatus = xPIDCompute_PostMeasurement(lux);
        
        if (xStatus != pdPASS)
        {
            DEBUG_PRINT("WARNING: Failed to post lux measurement to PID controller\n");
        }
        
        // Optional: Log readings periodically for debugging
        // DEBUG_PRINT("Lux: %lu (CH0: %u, CH1: %u)\n", lux, ch0, ch1);
        
        // Delay for the sample period
        vTaskDelay(xDelay);
    }
}

//********************* Public Interface **************************//

BaseType_t vLuxMngr_Init(uint32_t samplePeriodMs)
{
    BaseType_t xStatus;
    
    DEBUG_PRINT("Initializing Lux Manager (sample period: %lu ms)...\n", samplePeriodMs);
    
    // Store sample period
    ulSamplePeriodMs = samplePeriodMs;
    
    // Create the Lux Manager task
    xStatus = xTaskCreate(
        prvLuxMngr_Task,        // Task function
        "LuxMngr",              // Task name
        LUX_TASK_STACK_SIZE,    // Stack size
        NULL,                   // Parameters
        LUX_TASK_PRIORITY,      // Priority
        NULL                    // Task handle
    );
    
    if (xStatus != pdPASS)
    {
        DEBUG_PRINT("ERROR: Failed to create Lux Manager task\n");
        return pdFAIL;
    }
    
    DEBUG_PRINT("Lux Manager initialized successfully\n");
    return pdPASS;
}
