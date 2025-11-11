/* Sample FreeRTOS application

The application Demonstrates usage of Queues, Semaphores, Tasking model.

Application - Toggle LEDs on each button interrupt. Could modify to include switches

Flow diagram
GPIO Interrupt (BTN) --> ( ISR )Send a Semaphore --> Task 1 (Catch the Semaphore) -->
-->Task 1 - Send a Queue to Task -2 --> Task 2 Receive the queue --> Write to GPIO (LED)

Assumptions:
o Nexys4IO is connected to LEDs. (Could also use another GPIO instance
 o GPIO_1 is capable of generating an interrupt and is connect to the switches and buttons
(see project #2 write-up for details)
o AXI Timer 0 is a dual 32-bit timer with the Timer 0 interrupt used to generate
 the FreeRTOS systick.
*/

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "nexys4io.h"
#include <FreeRTOSConfig.h>
#include <stdlib.h>
#include <xgpio_l.h>
#include <xil_types.h>
#include <xstatus.h>

/* BSP includes. */
#include "xtmrctr.h"
#include "xgpio.h"
#include "sleep.h"



/*Definitions for NEXYS4IO Peripheral*/
#define N4IO_BASEADDR		    XPAR_NEXYS4IO_0_BASEADDR
#define N4IO_HIGHADDR		    XPAR_NEXYS4IO_0_HIGHADDR

/* GPIO definitions - use exact macros from xparameters.h */
#define GPIO_0_BASEADDR         XPAR_AXI_GPIO_0_BASEADDR
#define GPIO_INTERRUPT_ID       XPAR_FABRIC_AXI_GPIO_0_INTR // = 1
#define BTN_CHANNEL		1
#define SW_CHANNEL		2

#define mainQUEUE_LENGTH					( 1 )

/* A block time of 0 simply means, "don't block". */
#define mainDONT_BLOCK						( portTickType ) 0

//Create Instances
static XGpio xInputGPIOInstance;
static XGpio_Config *xInputGPIOConfig; 

//Function Declarations
static void prvSetupHardware( void );


//Declare a Semaphore
xSemaphoreHandle binary_sem;

/* The queue used by the queue send and queue receive tasks. */
static xQueueHandle xQueue = NULL;

//ISR, to handle interrupt of GPIO btns
//Give a Semaphore
static void gpio_intr (void *pvUnused)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    XGpio_InterruptClear(&xInputGPIOInstance, XGPIO_IR_MASK);
    xSemaphoreGiveFromISR(binary_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

//A task which takes the Interrupt Semaphore and sends a queue to task 2.
void sem_taken_que_tx (void *p)
{
    (void)p;  // Suppress unused parameter warning
    uint16_t ValueToSend = 0x00FF;

    while(1)
    if(xSemaphoreTake(binary_sem,500)){
        xil_printf("Queue Sent: %d\r\n",ValueToSend);
        xQueueSend( xQueue, &ValueToSend, mainDONT_BLOCK );
        ValueToSend = ~ValueToSend;//Toggle for next time.
    }
    else
        xil_printf("Semaphore time out\r\n");
}

void que_rx (void *p)
{
    (void)p;  // Suppress unused parameter warning
    uint16_t ReceivedValue;
    while(1){
        xQueueReceive( xQueue, &ReceivedValue, portMAX_DELAY );
        //Write to LED.
        NX4IO_setLEDs(ReceivedValue);
        xil_printf("Queue Received: %d\r\n",ReceivedValue);
    }
}

void post_start_IRQ_task(void *p) {
    (void)p; 
    XGpio_InterruptClear(&xInputGPIOInstance, XGPIO_IR_MASK);
    vPortEnableInterrupt( GPIO_INTERRUPT_ID );
    /* Enable GPIO channel interrupts on button channel & switchs */
    XGpio_InterruptEnable( &xInputGPIOInstance, XGPIO_IR_MASK);

    XGpio_InterruptGlobalEnable( &xInputGPIOInstance );
    vTaskDelete(NULL); 

}

int main(void)
{
    // Announcement
    xil_printf("Hello from FreeRTOS Example\r\n");

    //Initialize the HW
    prvSetupHardware();

    //Create Semaphore
    binary_sem = xSemaphoreCreateBinary();

    if(binary_sem == NULL) {
        xil_printf("Failed to create semaphore\n");
        return XST_FAILURE; 
    }

    /* Create the queue */
    xQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( uint16_t ) );

    /* Sanity check that the queue was created. */
    configASSERT( xQueue );

    //Create Task1
    BaseType_t task1Status = xTaskCreate( sem_taken_que_tx,
                 ( const char * ) "TX",
                 configMINIMAL_STACK_SIZE,
                 NULL,
                 1,
                 NULL );
    xil_printf("Task1 create status: %d\r\n", (int)task1Status);

    //Create Task2
    BaseType_t task2Status = xTaskCreate( que_rx,
                "RX",
                configMINIMAL_STACK_SIZE,
                NULL,
                2,
                NULL );
    xil_printf("Task2 create status: %d\r\n", (int)task2Status);

    // Create IRQ handle
    BaseType_t taskIrqStatus = xTaskCreate(post_start_IRQ_task,
                "IRQ_setup",
                configMINIMAL_STACK_SIZE,
                NULL,
                3,
                NULL );
    
    //Start the Scheduler
    xil_printf("Starting the scheduler\r\n");
    xil_printf("Push Button to change the LED pattern\r\n\r\n");
    vTaskStartScheduler();

    return -1;
}


static void prvSetupHardware( void )
{
    uint32_t xStatus;

    const unsigned char ucSetToInput = 0xFFU;

    xil_printf("Initializing GPIO's\r\n");


    /* Initialize the GPIO for the button inputs - use DEVICE ID not BASEADDR */
    xStatus = XGpio_Initialize( &xInputGPIOInstance, GPIO_0_BASEADDR );
    xil_printf("XGpio_Initialize status: %d\r\n", (int)xStatus);

    if( xStatus != XST_SUCCESS )
    {
        xil_printf("Button Initialization Failed\r\n");
    }    


    if( xStatus == XST_SUCCESS )
    {
        /* Install the handler defined in this task for the button input.
        *NOTE* The FreeRTOS defined xPortInstallInterruptHandler() API function
        must be used for this purpose. */
        xInputGPIOConfig = XGpio_LookupConfig(GPIO_0_BASEADDR);

        BaseType_t portStatus = xPortInstallInterruptHandler(xInputGPIOConfig->IntrId, (XInterruptHandler)gpio_intr, &xInputGPIOInstance);
        xil_printf("xPortInstallInterruptHandler status: %d\r\n", (int)portStatus);


        if( portStatus == pdPASS )
        {
            xil_printf("Buttons interrupt handler installed\r\n");

            /* Set switches and buttons to input. */
            XGpio_SetDataDirection( &xInputGPIOInstance, BTN_CHANNEL, ucSetToInput );
            XGpio_SetDataDirection( &xInputGPIOInstance, SW_CHANNEL, ucSetToInput );

            // /* Enable the button input interrupts in the interrupt controller.
            // *NOTE* The vPortEnableInterrupt() API function must be used for this
            // purpose. */
            // XGpio_InterruptClear(&xInputGPIOInstance, XGPIO_IR_CH1_MASK);
            // vPortEnableInterrupt( GPIO_INTERRUPT_ID );
            // /* Enable GPIO channel interrupts on button channel. Can modify to include switches */
            // XGpio_InterruptEnable( &xInputGPIOInstance, XGPIO_IR_CH1_MASK );
            // XGpio_InterruptGlobalEnable( &xInputGPIOInstance );
            
            xStatus = XST_SUCCESS;
        }
        else
        {
            xil_printf("Interrupt handler install FAILED\r\n");
            xStatus = XST_FAILURE;
        }

        // initialize the Nexys4 driver
        uint32_t status = NX4IO_initialize(N4IO_BASEADDR);
        xil_printf("NX4IO_initialize status: %d\r\n", (int)status);
        if (status != XST_SUCCESS){
            xil_printf("NX4IO initialization failed\r\n");
            xStatus = XST_FAILURE;
        }
    }

    xil_printf("prvSetupHardware complete, xStatus=%d\r\n", (int)xStatus);
    configASSERT( ( xStatus == XST_SUCCESS ) );
}
