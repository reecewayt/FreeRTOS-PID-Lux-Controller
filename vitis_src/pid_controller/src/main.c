/**
 * main.c - Initializes system and then hands off to FreeRTOS 
 * scheduler
 */
#include "FreeRTOS.h"
#include "task.h"
#include "xparameters.h"
#include "xgpio.h"
#include "led_pwm_mngr.h"

#define LOG_TAG "MAIN"
#include "logging.h" 

#include "xil_types.h"
#include "pid_mngr.h"
#include "tsl2561.h"
#include "xiic.h"

// --- Defines for the test task ---
#define TEST_TASK_STACK_SIZE   (configMINIMAL_STACK_SIZE)
#define TEST_TASK_PRIORITY     (tskIDLE_PRIORITY + 1)
#define TEST_TASK_DELAY_MS     (1000)

#define GPIO_0_BASEADDR         XPAR_AXI_GPIO_0_BASEADDR
#define GPIO_INTERRUPT_ID       XPAR_FABRIC_AXI_GPIO_0_INTR // = 1
#define BTN_CHANNEL		1
#define SW_CHANNEL		2

static XGpio xInputGPIOInstance;
static XGpio_Config *xInputGPIOConfig;

/**
 * @brief GPIO interrupt service routine
 * Called when buttons or switches change state
 */
static void gpio_isr(void *pvUnused)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    (void) pvUnused; 

    // Clear the interrupt
    XGpio_InterruptClear(&xInputGPIOInstance, XGPIO_IR_MASK);

    // Read the current state of switches and buttons
    uint16_t tempBtnState = XGpio_DiscreteRead(&xInputGPIOInstance, BTN_CHANNEL);
    uint16_t tempSwState = XGpio_DiscreteRead(&xInputGPIOInstance, SW_CHANNEL);

    // Package into user input state
    UserInputState_t newState;
    newState.sw = (uint8_t)tempSwState;
    newState.btn = (uint8_t)tempBtnState;

    // Post the new input state to the user input task (from ISR)
    xUserInput_PostFromISR(newState, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Initialize GPIO hardware for buttons and switches
 * Called before scheduler starts
 */
static void prvSetupGPIO(void)
{
    const unsigned char ucSetToInput = 0xFFU;
    uint32_t xStatus;

    DEBUG_PRINT("Initializing GPIO hardware...\n");

    // Initialize the GPIO instance
    xStatus = XGpio_Initialize(&xInputGPIOInstance, GPIO_0_BASEADDR);
    
    if (xStatus != XST_SUCCESS) {
        DEBUG_PRINT("ERROR: GPIO initialization failed\n");
        configASSERT(0);
    }

    // Get the GPIO configuration
    xInputGPIOConfig = XGpio_LookupConfig(GPIO_0_BASEADDR);
    
    if (xInputGPIOConfig == NULL) {
        DEBUG_PRINT("ERROR: GPIO config lookup failed\n");
        configASSERT(0);
    }

    // Install the interrupt handler
    BaseType_t portStatus = xPortInstallInterruptHandler(
        xInputGPIOConfig->IntrId, 
        (XInterruptHandler)gpio_isr, 
        &xInputGPIOInstance
    );
    
    if (portStatus != pdPASS) {
        DEBUG_PRINT("ERROR: Failed to install GPIO interrupt handler\n");
        configASSERT(0);
    }

    // Set buttons and switches to input
    XGpio_SetDataDirection(&xInputGPIOInstance, BTN_CHANNEL, ucSetToInput);
    XGpio_SetDataDirection(&xInputGPIOInstance, SW_CHANNEL, ucSetToInput);

    DEBUG_PRINT("GPIO hardware initialized successfully\n");
}

/**
 * @brief One-shot task to enable GPIO interrupts after scheduler starts
 * This must run after scheduler starts due to FreeRTOS requirements
 */
static void prvPostStartIRQ_Task(void *pvParameters)
{
    (void)pvParameters;

    DEBUG_PRINT("Enabling GPIO interrupts...\n");

    // Clear any pending interrupts
    XGpio_InterruptClear(&xInputGPIOInstance, XGPIO_IR_MASK);
    
    // Enable the interrupt in the interrupt controller
    vPortEnableInterrupt(GPIO_INTERRUPT_ID);
    
    // Enable GPIO channel interrupts for both buttons and switches
    XGpio_InterruptEnable(&xInputGPIOInstance, XGPIO_IR_MASK);
    
    // Enable global GPIO interrupts
    XGpio_InterruptGlobalEnable(&xInputGPIOInstance);

    DEBUG_PRINT("GPIO interrupts enabled\n");

    // Delete this task - it only needs to run once
    vTaskDelete(NULL);
}

/**
 * @brief Test task to post new PWM values to the LED manager
 */
//TODO: Remove this test task once integration is complete
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
            //DEBUG_PRINT("Posted new duty cycle: %d\n", (int)newDutyCycle);
        }

        // Increment the duty cycle. 
        // Note: u8 will automatically wrap from 224 + 32 = 256 back to 0.
        newDutyCycle += dutyCycleIncr;

        // Block this task for 1 second
        vTaskDelay(xDelay);
    }
}

/**
 * @brief Test task to verify TSL2561 sensor initialization
 * This is a one-shot task that initializes the sensor, reads the device ID,
 * reads both channels, computes lux, and then exits. For testing purposes only.
 */
static void prvTSL2561_Test_Task(void *pvParameters)
{
    (void)pvParameters;
    
    XIic i2cInstance;
    uint16_t ch0, ch1;
    uint32_t lux;
    
    DEBUG_PRINT("TSL2561 Test: Starting sensor test...\n");
    
    // Initialize the TSL2561 sensor
    tsl2561_init(&i2cInstance);
    
    DEBUG_PRINT("TSL2561 Test: Initialization complete\n");
    
    // Read channel 0 (broadband - visible + infrared)
    ch0 = tsl2561_readChannel(&i2cInstance, TSL2561_CHANNEL_0);
    DEBUG_PRINT("TSL2561 Test: Channel 0 = %u\n", ch0);
    
    // Read channel 1 (infrared only)
    ch1 = tsl2561_readChannel(&i2cInstance, TSL2561_CHANNEL_1);
    DEBUG_PRINT("TSL2561 Test: Channel 1 = %u\n", ch1);
    
    // Calculate lux value
    lux = tsl2561_calculateLux(ch0, ch1);
    DEBUG_PRINT("TSL2561 Test: Calculated Lux = %lu\n", lux);
    
    DEBUG_PRINT("TSL2561 Test: Test complete\n");
    
    // Task complete - delete itself
    vTaskDelete(NULL);
}

int main(void)
{
    DEBUG_PRINT("Hello welcome to project 2: ECE 544\n");

    // --- 1. Initialize GPIO hardware (before scheduler) ---
    prvSetupGPIO();

    // --- 2. Initialize PID Manager and create tasks ---
    vPID_TaskCreate();

    // --- 3. Initialize the LED PWM Manager task ---
    vLedPwm_TaskInit(); 

    // --- 4. Create the test task (temporary - for testing PWM) ---
    xTaskCreate(
        prvPwm_Task_Test,       // Pointer to the task function
        "PWM_Test",             // Text name for debugging
        TEST_TASK_STACK_SIZE,   // Stack depth
        NULL,                   // Task parameters (none)
        TEST_TASK_PRIORITY,     // Task priority
        NULL                    // Task handle (none)
    );

    // --- 5. Create one-shot task to enable interrupts after scheduler starts ---
    xTaskCreate(
        prvPostStartIRQ_Task,   // Task function
        "IRQ_Setup",            // Task name
        configMINIMAL_STACK_SIZE, // Stack size
        NULL,                   // Parameters
        tskIDLE_PRIORITY + 4,   // High priority to run first
        NULL                    // Task handle
    );

    // --- 6. Create TSL2561 test task (temporary - for testing sensor) ---
    xTaskCreate(
        prvTSL2561_Test_Task,   // Task function
        "TSL2561_Test",         // Task name
        configMINIMAL_STACK_SIZE, // Stack size
        NULL,                   // Parameters
        tskIDLE_PRIORITY + 2,   // Priority
        NULL                    // Task handle
    );

    // --- 7. Start the FreeRTOS scheduler ---
    DEBUG_PRINT("Starting FreeRTOS scheduler...\n");
    vTaskStartScheduler();

    // The scheduler is running, so we should never get here
    for (;;);

    return 0; // Should not be reached
}