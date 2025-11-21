/**
 * @file main.c
 * @brief FreeRTOS-based PID Lux Controller Application
 * 
 * This embedded system implements a closed-loop PID controller that maintains a target
 * light level (lux) by adjusting LED brightness via PWM. The application runs on a
 * MicroBlaze soft processor with FreeRTOS for task management.
 * 
 * SYSTEM ARCHITECTURE:
 * 
 * 1. Lux Manager Task (lux_mngr.c)
 *    - Periodically reads TSL2561 light sensor via I2C (configurable sample rate)
 *    - Calculates lux value from raw sensor data
 *    - Posts measurements to PID Compute Task via queue
 * 
 * 2. PID Manager Tasks (pid_mngr.c)
 *    a) User Input Task:
 *       - Processes GPIO interrupts from switches and buttons
 *       - Configures PID parameters (Kp, Ki, Kd gains and setpoint)
 *       - Enables/disables P, I, D control terms
 *       - Adjustable step increments (±1, ±5, ±10)
 *    
 *    b) PID Compute Task:
 *       - Receives lux measurements from Lux Manager
 *       - Executes PID control algorithm with integer arithmetic
 *       - Applies output filtering for smooth PWM transitions
 *       - Posts PWM duty cycle (0-255) to LED PWM Manager
 *       - Sends control data to Log Manager
 * 
 * 3. LED PWM Manager Task (led_pwm_mngr.c)
 *    - Receives PWM duty cycle commands via queue
 *    - Controls RGB LED blue channel via Nexys4IO hardware PWM
 *    - Adjusts LED brightness to maintain target lux level
 * 
 * 4. Log Manager Task (log_mngr.c)
 *    - Receives control system state snapshots via queue
 *    - Outputs CSV-formatted data for analysis (timestamp, setpoint, measured lux,
 *      gains, P/I/D terms, PWM output)
 *    - Updates 7-segment display showing setpoint and current lux reading
 * 
 * HARDWARE INTERFACES:
 * - TSL2561 Light Sensor (I2C)
 * - Nexys4 GPIO (buttons and switches for user input)
 * - Nexys4IO Custom Peripheral (RGB LED PWM, 7-segment display) provided by ECE 544 professor
 * 
 * 
 * CONTROL FEATURES:
 * - Integer-based PID with configurable gains
 * - Integral anti-windup protection
 * - Output saturation (0-255 for 8-bit PWM)
 * - Exponential moving average output filtering
 * - Individual P/I/D term enable/disable
 * - Real-time parameter tuning via hardware interface
 * 
 * @author Reece Wayt & Marco Martinez
 * @date 2025
 * @note Code was developed with assistance from Github Copilot
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
#include "lux_mngr.h"
#include "log_mngr.h"


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
    newState.sw = (uint8_t)tempSwState;     // uses only lower 8 bits for our app
    newState.btn = (uint8_t)tempBtnState;

    // Post the new input state to the user input task (from ISR)
    // this will unblock the task if it is waiting
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



int main(void)
{
    DEBUG_PRINT("Hello welcome to project 2: ECE 544\n");

    prvSetupGPIO();

    vPID_TaskCreate();

    vLedPwm_TaskInit();

    if (vLogMngr_Init() != pdPASS) {
        DEBUG_PRINT("WARNING: Failed to initialize Log Manager\n");
    } 
    
    if (vLuxMngr_Init(100) != pdPASS) {
        DEBUG_PRINT("WARNING: Failed to initialize Lux Manager\n");
    }

    
    xTaskCreate(
        prvPostStartIRQ_Task,   // Task function
        "IRQ_Setup",            // Task name
        configMINIMAL_STACK_SIZE, // Stack size
        NULL,                   // Parameters
        tskIDLE_PRIORITY + 4,   // High priority to run first
        NULL                    // Task handle
    );

    
    DEBUG_PRINT("Starting FreeRTOS scheduler...\n");
    vTaskStartScheduler();

    // The scheduler is running, so we should never get here
    for (;;);

    return 0; // Should not be reached
}