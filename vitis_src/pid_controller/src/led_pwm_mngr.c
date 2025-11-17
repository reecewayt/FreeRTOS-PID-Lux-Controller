#include "led_pwm_mngr.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"


#include "nexys4io.h" 
#include "xparameters.h"
#include "xil_types.h"
#include "queue.h"

#define LOG_TAG "LED_PWM"
#include "logging.h" 


#define LED_TASK_STACK_SIZE   (configMINIMAL_STACK_SIZE)
#define LED_TASK_PRIORITY     (tskIDLE_PRIORITY + 1)

#define RGB_1_CHANNEL           1
#define LED_PWM_QUEUE_LENGTH    5

static xQueueHandle xPwmQueue = NULL; 


/**
 * @brief The main task routine for managing the LED PWM.
 */
static void prvPwm_Task(void *pvParameters)
{
    (void) pvParameters; 

    u32 status = NX4IO_initialize(XPAR_NEXYS4IO_0_BASEADDR); 
    
    if ( status != XST_SUCCESS) {
        DEBUG_PRINT("NX4IO failed to initialize: %d\n", (int)status);
        // We probably should not continue if this fails
        vTaskDelete(NULL); // Delete this task
    }

    // enable pwm in control register for blue channel
    NX4IO_RGBLED_setChnlEn(RGB_1_CHANNEL, false, false, true); 


    u8 u8NewDutyCycle; // Variable to hold the duty cycle received from the queue

    for( ;; ) 
    {
        // Wait block indefinitely (portMAX_DELAY) until a new item
        // arrives in the queue.
        //
        if (xQueueReceive(xPwmQueue, &u8NewDutyCycle, portMAX_DELAY) == pdPASS)
        {
            // Received a new duty cycle.
            NX4IO_RGBLED_setDutyCycle(RGB_1_CHANNEL, 0, 0, u8NewDutyCycle);
        }
        // If xQueueReceive fails (e.g., queue deleted), the loop
        // will just repeat and block again.
    }
}

// -----------------------------------------------------------------
// PUBLIC FUNCTIONS 
// -----------------------------------------------------------------

/**
 * @brief Initializes and creates the LED PWM task.
 */
void vLedPwm_TaskInit(void) 
{
    xPwmQueue = xQueueCreate(LED_PWM_QUEUE_LENGTH, sizeof(u8));

    if (xPwmQueue == NULL)
    {
        // Failed to create the queue
        DEBUG_PRINT("Failed to create xPwmQueue\n");
        return; // Don't create the task
    }

    // Create the task
    xTaskCreate(
        prvPwm_Task,              // [1] Pointer to the task function
        "LED_Pwm_Manager",        // [2] Text name for debugging
        LED_TASK_STACK_SIZE,      // [3] Stack depth
        NULL,                     // [4] Task parameters (none)
        LED_TASK_PRIORITY,        // [5] Task priority
        NULL                      // [6] Task handle (none)
    );
}

/**
 * @brief Posts a new duty cycle to the LED PWM task's queue.
 */
BaseType_t xLEDPwm_PostDutyCycle(uint8_t u8DutyCycle) 
{
    // This function is "thread-safe" (can be called from any task).
    //
    // We use a 0-tick block time (last parameter). This means
    // if the queue is full, it returns immediately instead of waiting.
    return xQueueSend(xPwmQueue, &u8DutyCycle, (TickType_t)0);
}