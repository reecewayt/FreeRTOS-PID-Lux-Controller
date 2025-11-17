/**
 * main.c - Initializes system and then hands off to FreeRTOS 
 * scheduler
 */
#include "FreeRTOS.h"
#include "task.h" // <-- ADDED: Needed for task functions and scheduler
#include "xparameters.h"
#include "led_pwm_mngr.h"

#define LOG_TAG "MAIN"
#include "logging.h" 

#include "xil_types.h"

// --- Defines for the test task ---
#define TEST_TASK_STACK_SIZE   (configMINIMAL_STACK_SIZE)
#define TEST_TASK_PRIORITY     (tskIDLE_PRIORITY + 1)
#define TEST_TASK_DELAY_MS     (1000)

/**
 * @brief Test task to post new PWM values to the LED manager
 */
static void prvPwm_Task_Test(void *pvParameters)
{
    // Cast unused parameter to void to prevent compiler warnings
    (void)pvParameters; 
    
    u8 newDutyCycle = 0; 
    u8 dutyCycleIncr = 32;   // We will cycle through 8 light intensity values
    TickType_t xDelay = pdMS_TO_TICKS(TEST_TASK_DELAY_MS);

    DEBUG_PRINT("Starting PWM test task...\n");

    for (;;) {
        // Post the new duty cycle to the LED manager's queue
        if (xLEDPwm_PostDutyCycle(newDutyCycle) != pdPASS)
        {
            DEBUG_PRINT("Failed to post to PWM queue (queue full?)\n");
        }
        else
        {
            DEBUG_PRINT("Posted new duty cycle: %d\n", (int)newDutyCycle);
        }

        // Increment the duty cycle. 
        // Note: u8 will automatically wrap from 224 + 32 = 256 back to 0.
        newDutyCycle += dutyCycleIncr;

        // Block this task for 1 second
        vTaskDelay(xDelay);
    }
}

int main(void)
{
    DEBUG_PRINT("Hello welcome to project 2: ECE 544\n");

    // --- 1. Initialize all modules/tasks ---
    
    // Initialize the LED PWM Manager task (the consumer)
    // This function should create the queue and the LED task.
    vLedPwm_TaskInit(); 

    // --- 2. Create the test task (the producer) ---
    xTaskCreate(
        prvPwm_Task_Test,       // Pointer to the task function
        "PWM_Test",             // Text name for debugging
        TEST_TASK_STACK_SIZE,   // Stack depth
        NULL,                   // Task parameters (none)
        TEST_TASK_PRIORITY,     // Task priority
        NULL                    // Task handle (none)
    );

    // --- 3. Start the FreeRTOS scheduler ---
    DEBUG_PRINT("Starting FreeRTOS scheduler...\n");
    vTaskStartScheduler();

    // The scheduler is running, so we should never get here
    for (;;);

    return 0; // Should not be reached
}