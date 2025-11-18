#include "log_mngr.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "xil_printf.h"
#include "nexys4io.h"

//********************* Configuration *****************************//

#define LOG_TASK_STACK_SIZE     (configMINIMAL_STACK_SIZE * 2)
#define LOG_TASK_PRIORITY       (tskIDLE_PRIORITY + 1)
#define LOG_QUEUE_LENGTH        10

//********************* Private Variables *************************//

static QueueHandle_t xLogQueue = NULL;
static uint32_t ulStartTicks = 0;

//********************* Private Functions *************************//

/**
 * @brief Print CSV header to console
 */
static void prvPrintCSVHeader(void)
{
    xil_printf("Timestamp_ms,Setpoint,Measured,Kp,Ki,Kd,P_term,I_term,D_term,PWM_Output\r\n");
}

/**
 * @brief Format and print log data as CSV
 */
static void prvPrintLogData(const LogData_t *data)
{
    xil_printf("%lu,%lu,%lu,%lu,%lu,%lu,%ld,%ld,%ld,%u\r\n",
               data->timestamp_ms,
               data->lux_setpoint,
               data->lux_measured,
               data->kp,
               data->ki,
               data->kd,
               data->p_term,
               data->i_term,
               data->d_term,
               data->pid_output);
}

/**
 * @brief Update seven segment display with setpoint and measured lux values
 * 
 * Display layout:
 * Bank SSEGHI: [Digit 7] [Digit 6] [Digit 5] [Digit 4]
 * Bank SSEGLO: [Digit 3] [Digit 2] [Digit 1] [Digit 0]
 * 
 * Mapping:
 * Digits 7-5: Setpoint (0-999)
 * Digit 4:    Blank
 * Digits 3-1: Measured Lux (0-999)
 * Digit 0:    Blank
 * 
 * @param setpoint     Target lux value (0-999)
 * @param measured_lux Current lux reading (0-999)
 */
static void prvUpdateSevenSegmentDisplay(uint32_t setpoint, uint32_t measured_lux)
{
    u8 sp_hundreds, sp_tens, sp_ones;
    u8 lux_hundreds, lux_tens, lux_ones;
    u8 blank = CC_BLANK;
    
    // 1. Process Setpoint (Digits 7-5)
    // Clamp to 999 if out of range
    if (setpoint > 999) { setpoint = 999; }
    sp_hundreds = (u8)(setpoint / 100);
    sp_tens     = (u8)((setpoint % 100) / 10);
    sp_ones     = (u8)(setpoint % 10);
    
    // 2. Process Measured Lux (Digits 3-1)
    if (measured_lux > 999) { measured_lux = 999; }
    lux_hundreds = (u8)(measured_lux / 100);
    lux_tens     = (u8)((measured_lux % 100) / 10);
    lux_ones     = (u8)(measured_lux % 10);
    
    // 3. Update the display banks
    // Bank SSEGHI: [Digit 7] [Digit 6] [Digit 5] [Digit 4]
    // Mapped to:   [sp_hundreds] [sp_tens] [sp_ones] [blank]
    NX410_SSEG_setAllDigits(SSEGHI, sp_hundreds, sp_tens, sp_ones, blank, DP_NONE);
    
    // Bank SSEGLO: [Digit 3] [Digit 2] [Digit 1] [Digit 0]
    // Mapped to:   [lux_hundreds] [lux_tens] [lux_ones] [blank]
    NX410_SSEG_setAllDigits(SSEGLO, lux_hundreds, lux_tens, lux_ones, blank, DP_NONE);
}

//********************* Log Manager Task **************************//

/**
 * @brief Log Manager task implementation
 * 
 * This task waits for log data from the control system and prints it
 * in CSV format to the console for analysis and graphing.
 */
static void prvLogMngr_Task(void *pvParameters)
{
    (void)pvParameters;
    
    LogData_t logData;
    
    // Store start time for relative timestamps
    ulStartTicks = xTaskGetTickCount();
    
    // Print CSV header
    #ifndef DEBUG_1
        prvPrintCSVHeader();
    #endif
    
    for (;;)
    {
        // Block waiting for new log data
        if (xQueueReceive(xLogQueue, &logData, portMAX_DELAY) == pdTRUE)
        {
            #ifndef DEBUG_1 // Only print log data when debug logging is disabled
                            // to avoid cluttering debug output
                prvPrintLogData(&logData);
            #endif
            
            // Update seven segment display with current setpoint and lux reading
            prvUpdateSevenSegmentDisplay(logData.lux_setpoint, logData.lux_measured);
        }
    }
}

//********************* Public Interface **************************//

BaseType_t vLogMngr_Init(void)
{
    BaseType_t xStatus;
    
    // Create log data queue
    xLogQueue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(LogData_t));
    if (xLogQueue == NULL)
    {
        return pdFAIL;
    }
    
    // Create the Log Manager task
    xStatus = xTaskCreate(
        prvLogMngr_Task,        // Task function
        "LogMngr",              // Task name
        LOG_TASK_STACK_SIZE,    // Stack size
        NULL,                   // Parameters
        LOG_TASK_PRIORITY,      // Priority (low - logging is not time-critical)
        NULL                    // Task handle
    );
    
    if (xStatus != pdPASS)
    {
        vQueueDelete(xLogQueue);
        return pdFAIL;
    }
    
    return pdPASS;
}

BaseType_t xLogMngr_Post(const LogData_t *logData)
{
    if (xLogQueue == NULL || logData == NULL)
    {
        return pdFAIL;
    }
    
    // Send with 0 timeout - don't block if queue is full
    return xQueueSend(xLogQueue, logData, 0);
}

BaseType_t xLogMngr_PostFromISR(const LogData_t *logData, BaseType_t *pxHigherPriorityTaskWoken)
{
    if (xLogQueue == NULL || logData == NULL)
    {
        return pdFAIL;
    }
    
    return xQueueSendFromISR(xLogQueue, logData, pxHigherPriorityTaskWoken);
}
