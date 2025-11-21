#include "pid_controller.h"
#include "pid_mngr.h"
#include "led_pwm_mngr.h"
#include "log_mngr.h"

#define LOG_TAG "PID_MNGR"
#include "logging.h" 

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//********************* User Input Bit Masks & Macros *********************//

// Switch bit positions
#define SW_ENABLE_P         (1 << 0)  // Switch[0] - Enable Proportional
#define SW_ENABLE_I         (1 << 1)  // Switch[1] - Enable Integral
#define SW_ENABLE_D         (1 << 2)  // Switch[2] - Enable Derivative
#define SW_SETPOINT_SELECT  (1 << 3)  // Switch[3] - Select setpoint adjustment
#define SW_STEP_BIT0        (1 << 4)  // Switch[4] - Step increment bit 0
#define SW_STEP_BIT1        (1 << 5)  // Switch[5] - Step increment bit 1
#define SW_GAIN_SELECT_BIT0 (1 << 6)  // Switch[6] - Gain select bit 0
#define SW_GAIN_SELECT_BIT1 (1 << 7)  // Switch[7] - Gain select bit 1

// Button bit positions (assuming buttons are in lower 5 bits of 16-bit GPIO)
#define BTN_R               (1 << 0)  // Right button
#define BTN_L               (1 << 1)  // Left button
#define BTN_D               (1 << 2)  // Down button
#define BTN_U               (1 << 3)  // Up button
#define BTN_C               (1 << 4)  // Center button

// Macros to extract user input fields
#define GET_GAIN_SELECT(sw)     (((sw) >> 6) & 0x03)  // Extract bits [7:6]
#define GET_STEP_SELECT(sw)     (((sw) >> 4) & 0x03)  // Extract bits [5:4]
#define GET_SETPOINT_SELECT(sw) (((sw) >> 3) & 0x01)  // Extract bit [3]
#define GET_PID_ENABLES(sw)     ((sw) & 0x07)         // Extract bits [2:0]

// Gain selection values (from switches[7:6])
#define GAIN_SELECT_NONE    0x00  // 00: No gain selected
#define GAIN_SELECT_KP      0x01  // 01: Kp selected
#define GAIN_SELECT_KI      0x02  // 10: Ki selected
#define GAIN_SELECT_KD      0x03  // 11: Kd selected

// Step increment values (from switches[5:4])
#define STEP_SELECT_1       0x00  // 00: ±1 increment
#define STEP_SELECT_5       0x01  // 01: ±5 increment
#define STEP_SELECT_10      0x02  // 10 or 11: ±10 increment 

static PID_Controller _pid;    // private pid instance

static QueueHandle_t xUserInputQueue = NULL;          // ISR → UserInputTask
static QueueHandle_t xLuxMeasurementQueue = NULL;     // Sensor Task/ISR → ComputeTask -> Led PWM Task

static SemaphoreHandle_t xPIDMutex = NULL;            // Mutex to protect PID controller state

typedef enum {
    BUTTON_UP_PRESSED,
    BUTTON_DOWN_PRESSED
} InputEvent_t;

typedef enum {
    DO_NOTHING = 0,
    KP = 1,
    KI = 2,
    KD = 3
} GainType_t; 

// Forward declarations of private helper functions
static void _enablePIDGainConst(GainType_t gain_const);
static void _disablePIDGainConst(GainType_t gain_const);
static void vUserInput_Task(void *pvParameters);
static void vPIDCompute_Task(void *pvParameters);
static inline uint32_t _clamp_add_uint32(uint32_t value, int32_t delta);



//********************* RTOS Interface to PID controller ************//
static void vUserInput_TaskInit(void) {
    // Create user input queue
    xUserInputQueue = xQueueCreate(10, sizeof(UserInputState_t));
    if (xUserInputQueue == NULL) {
        DEBUG_PRINT("Failed to create User Input Queue\n");
        return;
    }

    // Create User Input Task
    BaseType_t result = xTaskCreate(
        vUserInput_Task,           // Task function
        "PID_UserInput",           // Task name
        configMINIMAL_STACK_SIZE * 2,  // Stack size
        NULL,                      // Parameters
        tskIDLE_PRIORITY + 2,      // Priority (medium)
        NULL                       // Task handle
    );
    
    if(result != pdPASS) {
        DEBUG_PRINT("Failed to create User Input Task\n");
    } else {
        DEBUG_PRINT("User Input Task created successfully\n");
    }
}

static void vPIDCompute_TaskInit(void) {
    // Create lux measurement queue
    xLuxMeasurementQueue = xQueueCreate(5, sizeof(uint32_t));
    if (xLuxMeasurementQueue == NULL) {
        DEBUG_PRINT("Failed to create Lux Measurement Queue\n");
        return;
    }

    // Create PID Compute Task
    BaseType_t result = xTaskCreate(
        vPIDCompute_Task,          // Task function
        "PID_Compute",             // Task name
        configMINIMAL_STACK_SIZE * 2,  // Stack size
        NULL,                      // Parameters
        tskIDLE_PRIORITY + 3,      // Priority (high - control loop)
        NULL                       // Task handle
    );
    
    if(result != pdPASS) {
        DEBUG_PRINT("Failed to create PID Compute Task\n");
    } else {
        DEBUG_PRINT("PID Compute Task created successfully\n");
    }
}

void vPID_TaskCreate(void) {
    // Initialize PID controller with defaults
    PID_Init(&_pid);
    
    // Configure initial parameters 
    _pid.config.kp = 0;              
    _pid.config.ki = 0;               
    _pid.config.kd = 0;                
    _pid.config.setpoint = 50;         
    _pid.config.max_setpoint = PID_DEFAULT_MAX_SETPOINT; 
    _pid.config.output_min = 0;         // PWM min
    _pid.config.output_max = 255;       // PWM max
    _pid.config.integral_min = PID_INTEGRAL_MIN; // Integral anti-windup min
    _pid.config.integral_max = PID_INTEGRAL_MAX;  // Integral anti-windup max
    _pid.config.enable_flags = ~(PID_ENABLE_ALL);  // Disable all terms
    _pid.config.step = LOW_INCR;        // Default step increment +/-1
    
    DEBUG_PRINT("PID Controller initialized\n");
    
    // Create mutex needed by both tasks
    xPIDMutex = xSemaphoreCreateMutex();
    if (xPIDMutex == NULL) {
        DEBUG_PRINT("Failed to create PID Mutex\n");
        return;
    }
    DEBUG_PRINT("PID Mutex created\n");
    
    // Create tasks
    vUserInput_TaskInit();
    vPIDCompute_TaskInit();
}

BaseType_t xUserInput_Post(UserInputState_t pxNewState) {
    if(xUserInputQueue == NULL) {
        return pdFAIL;
    }
    
    return xQueueSend(xUserInputQueue, &pxNewState, 0);
}

BaseType_t xUserInput_PostFromISR(UserInputState_t pxNewState, BaseType_t *pxHigherPriorityTaskWoken) {
    if(xUserInputQueue == NULL) {
        return pdFAIL;
    }
    
    return xQueueSendFromISR(xUserInputQueue, &pxNewState, pxHigherPriorityTaskWoken);
}

BaseType_t xPIDCompute_PostMeasurement(uint32_t luxMeasurement) {
    if(xLuxMeasurementQueue == NULL) {
        return pdFAIL;
    }
    
    return xQueueSend(xLuxMeasurementQueue, &luxMeasurement, 0);
}

/**
 * @brief User Input Task - Processes switch and button inputs to configure PID
 * 
 * This task reads user input from a queue and updates PID configuration:
 * - Switches[7:6]: Select gain to adjust (Kp, Ki, Kd)
 * - Switches[5:4]: Select step increment (1, 5, 10)
 * - Switch[3]: Enable setpoint adjustment
 * - Switches[2:0]: Enable/disable P, I, D terms
 * - BtnU/BtnD: Increment/decrement selected parameter
 */
static void vUserInput_Task(void *pvParameters) {

    (void) pvParameters; 

    UserInputState_t input;
    UserInputState_t prev_input = {0, 0};  // Track previous state for edge detection
    
    DEBUG_PRINT("User Input Task started\n");
    
    for(;;) {
        // Block waiting for user input
        if(xQueueReceive(xUserInputQueue, &input, portMAX_DELAY) == pdTRUE) {
            
            // Acquire mutex before modifying PID state
            if(xSemaphoreTake(xPIDMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                
                // ========== Process Step Size Selection [5:4] ========== 
                uint8_t step_select = GET_STEP_SELECT(input.sw);
                step_incr_t new_step;
                
                if(step_select >= STEP_SELECT_10) {
                    new_step = HIGH_INCR;  // 10 or 11 -> ±10
                } else if(step_select == STEP_SELECT_5) {
                    new_step = MED_INCR;   // 01 -> ±5
                } else {
                    new_step = LOW_INCR;   // 00 -> ±1
                }
                
                if(new_step != _pid.config.step) {
                    _pid.config.step = new_step;
                    DEBUG_PRINT("Step increment changed to %d\n", new_step);
                }
                
                // ========== Process PID Enable Flags [2:0] ==========
                uint8_t pid_enables = GET_PID_ENABLES(input.sw);
                uint8_t prev_enables = GET_PID_ENABLES(prev_input.sw);
                
                if(pid_enables != prev_enables) {
                    // Check each enable bit
                    if((pid_enables & SW_ENABLE_P) && !(prev_enables & SW_ENABLE_P)) {
                        _enablePIDGainConst(KP);
                        DEBUG_PRINT("P term enabled\n");
                    } else if(!(pid_enables & SW_ENABLE_P) && (prev_enables & SW_ENABLE_P)) {
                        _disablePIDGainConst(KP);
                        DEBUG_PRINT("P term disabled\n");
                    }
                    
                    if((pid_enables & SW_ENABLE_I) && !(prev_enables & SW_ENABLE_I)) {
                        _enablePIDGainConst(KI);
                        DEBUG_PRINT("I term enabled\n");
                    } else if(!(pid_enables & SW_ENABLE_I) && (prev_enables & SW_ENABLE_I)) {
                        _disablePIDGainConst(KI);
                        DEBUG_PRINT("I term disabled\n");
                    }
                    
                    if((pid_enables & SW_ENABLE_D) && !(prev_enables & SW_ENABLE_D)) {
                        _enablePIDGainConst(KD);
                        DEBUG_PRINT("D term enabled\n");
                    } else if(!(pid_enables & SW_ENABLE_D) && (prev_enables & SW_ENABLE_D)) {
                        _disablePIDGainConst(KD);
                        DEBUG_PRINT("D term disabled\n");
                    }
                }
                
                // ========== Process Button Presses (Positive Edge Detection) ==========
                uint8_t btn_up_pressed = (input.btn & BTN_U) && !(prev_input.btn & BTN_U);
                uint8_t btn_down_pressed = (input.btn & BTN_D) && !(prev_input.btn & BTN_D);
                
                if(btn_up_pressed || btn_down_pressed) {
                    // Calculate increment direction
                    int32_t incr = (int32_t)_pid.config.step;
                    if(btn_down_pressed) {
                        incr = -incr;
                    }
                    
                    // Check if setpoint adjustment is selected [3]
                    if(GET_SETPOINT_SELECT(input.sw)) {
                        _pid.config.setpoint = _clamp_add_uint32(_pid.config.setpoint, incr);
                        
                        if(_pid.config.setpoint == 0) {
                            DEBUG_PRINT("Warning, setpoint is at zero\n");
                        } else if(_pid.config.setpoint >= _pid.config.max_setpoint) {
                            _pid.config.setpoint = _pid.config.max_setpoint;
                            DEBUG_PRINT("Warning, setpoint at max: %u\n", _pid.config.max_setpoint);
                        }
                        
                        DEBUG_PRINT("Setpoint adjusted to: %u\n", _pid.config.setpoint);
                    } 
                    // Otherwise, adjust selected gain constant [7:6]
                    else {
                        uint8_t gain_select = GET_GAIN_SELECT(input.sw);
                        
                        switch(gain_select) {
                            case GAIN_SELECT_KP:
                                _pid.config.kp = _clamp_add_uint32(_pid.config.kp, incr);
                                DEBUG_PRINT("Kp adjusted to: %d\n", _pid.config.kp);
                                break;
                                
                            case GAIN_SELECT_KI:
                                _pid.config.ki = _clamp_add_uint32(_pid.config.ki, incr);
                                DEBUG_PRINT("Ki adjusted to: %d\n", _pid.config.ki);
                                break;
                                
                            case GAIN_SELECT_KD:
                                _pid.config.kd = _clamp_add_uint32(_pid.config.kd, incr);
                                DEBUG_PRINT("Kd adjusted to: %d\n", _pid.config.kd);
                                break;
                                
                            case GAIN_SELECT_NONE:
                            default:
                                DEBUG_PRINT("No gain selected for adjustment\n");
                                break;
                        }
                    }
                }
                
                // Store current input for next iteration
                prev_input = input;
                
                xSemaphoreGive(xPIDMutex);
            } else {
                DEBUG_PRINT("Failed to acquire PID mutex in User Input Task\n");
            }
        }
    }
}

/**
 * @brief PID Compute Task - Processes lux measurements and computes PID output
 * 
 * This task reads lux measurements from a queue, computes the PID control output,
 * and returns the PWM duty cycle (0-255) for LED brightness control.
 */
static void vPIDCompute_Task(void *pvParameters) {

    (void) pvParameters; 

    uint32_t lux_measurement;
    uint8_t pwm_duty;
    
    DEBUG_PRINT("PID Compute Task started\n");
    
    for(;;) {
        // Block waiting for new lux measurement
        if(xQueueReceive(xLuxMeasurementQueue, &lux_measurement, portMAX_DELAY) == pdTRUE) {
            
            // Acquire mutex before accessing PID state
            if(xSemaphoreTake(xPIDMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                
                // Enter critical section - disable interrupts for atomic PID computation
                taskENTER_CRITICAL();
                
                // Compute PID output (critical - must be atomic)
                pwm_duty = PID_Compute(&_pid, lux_measurement);
                
                // Prepare log data while we have mutex and interrupts disabled
                LogData_t logData;
                logData.timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                logData.lux_setpoint = _pid.config.setpoint;
                logData.lux_measured = lux_measurement;
                logData.kp = _pid.config.kp;
                logData.ki = _pid.config.ki;
                logData.kd = _pid.config.kd;
                logData.p_term = _pid.p_term;
                logData.i_term = _pid.i_term;
                logData.d_term = _pid.d_term;
                logData.pid_output = pwm_duty;
                
                // Exit critical section - re-enable interrupts
                taskEXIT_CRITICAL();
                
                xSemaphoreGive(xPIDMutex);
                
                // Send PWM duty cycle to LED PWM manager task
                BaseType_t pwm_status = xLEDPwm_PostDutyCycle(pwm_duty);
                
                if (pwm_status != pdPASS) {
                    DEBUG_PRINT("WARNING: Failed to post PWM duty cycle to LED manager\n");
                }
                
                // Post data to logging task
                xLogMngr_Post(&logData);
                
                // Log PID computation details (debug only)
                DEBUG_PRINT("Lux=%u, Setpoint=%u, PWM=%u, P=%d, I=%d, D=%d\n", 
                           lux_measurement, _pid.config.setpoint, pwm_duty,
                           _pid.p_term, _pid.i_term, _pid.d_term);
                
            } else {
                DEBUG_PRINT("Failed to acquire PID mutex in Compute Task\n");
            }
        }
    }
}

//********************* Private Utility Functions *******************//


/**
 * @brief Clamp an unsigned value, preventing underflow to zero
 * @param value Current value
 * @param delta Change to apply (can be negative)
 * @return Clamped result, minimum 0
 */
static inline uint32_t _clamp_add_uint32(uint32_t value, int32_t delta) {
    int32_t result = (int32_t)value + delta;
    return (result < 0) ? 0 : (uint32_t)result;
}

static void _disablePIDGainConst(GainType_t gain_const) {
    uint8_t flag_mask = 0;
    
    switch(gain_const) {
        case KP:
            flag_mask = PID_ENABLE_P;
            break;
        case KI:
            flag_mask = PID_ENABLE_I;
            /* Clear integral when disabling to prevent windup */
            _pid.integral = 0;
            _pid.i_term = 0;
            break;
        case KD:
            flag_mask = PID_ENABLE_D;
            break;
        case DO_NOTHING:
        default:
            return;
    }
    
    // Clear corresponding enable flag
    _pid.config.enable_flags &= ~flag_mask;
}

static void _enablePIDGainConst(GainType_t gain_const) {
    uint8_t flag_mask = 0;
    
    switch(gain_const) {
        case KP:
            flag_mask = PID_ENABLE_P;
            break;
        case KI:
            flag_mask = PID_ENABLE_I;
            break;
        case KD:
            flag_mask = PID_ENABLE_D;
            break;
        case DO_NOTHING:
        default:
            return;
    }
    
    // Set corresponding enable flag
    _pid.config.enable_flags |= flag_mask;
}



