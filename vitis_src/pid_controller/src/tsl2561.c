#include "tsl2561.h"
#include "xparameters.h"
#include "logging.h"
#include "xiic_l.h"
#include <unistd.h>

// TSL2561 I2C Address
#define TSL2561_ADDR         0x39  // Default I2C address

// TSL2561 Register Definitions
#define TSL2561_COMMAND_BIT          0x80
#define TSL2561_WORD_BIT             0x20
#define TSL2561_REGISTER_CONTROL     0x00
#define TSL2561_REGISTER_TIMING      0x01
#define TSL2561_REGISTER_ID          0x0A
#define TSL2561_REGISTER_CHAN0_LOW   0x0C
#define TSL2561_REGISTER_CHAN0_HIGH  0x0D
#define TSL2561_REGISTER_CHAN1_LOW   0x0E
#define TSL2561_REGISTER_CHAN1_HIGH  0x0F

// TSL2561 Control Values
#define TSL2561_CONTROL_POWERON      0x03
#define TSL2561_CONTROL_POWEROFF     0x00

// TSL2561 Integration Time (using 13ms for minimal implementation)
#define TSL2561_INTEGRATIONTIME_13MS 0x00
#define TSL2561_DELAY_INTTIME_13MS   15  // milliseconds

// TSL2561 Gain (using 1x for minimal implementation)
#define TSL2561_GAIN_1X              0x00

// Lux Calculation Constants (T/FN/CL package)
#define TSL2561_LUX_LUXSCALE         14
#define TSL2561_LUX_RATIOSCALE       9
#define TSL2561_LUX_CHSCALE          10
#define TSL2561_LUX_CHSCALE_TINT0    0x7517

#define TSL2561_LUX_K1T              0x0040
#define TSL2561_LUX_B1T              0x01f2
#define TSL2561_LUX_M1T              0x01be
#define TSL2561_LUX_K2T              0x0080
#define TSL2561_LUX_B2T              0x0214
#define TSL2561_LUX_M2T              0x02d1
#define TSL2561_LUX_K3T              0x00c0
#define TSL2561_LUX_B3T              0x023f
#define TSL2561_LUX_M3T              0x037b
#define TSL2561_LUX_K4T              0x0100
#define TSL2561_LUX_B4T              0x0270
#define TSL2561_LUX_M4T              0x03fe
#define TSL2561_LUX_K5T              0x0138
#define TSL2561_LUX_B5T              0x016f
#define TSL2561_LUX_M5T              0x01fc
#define TSL2561_LUX_K6T              0x019a
#define TSL2561_LUX_B6T              0x00d2
#define TSL2561_LUX_M6T              0x00fb
#define TSL2561_LUX_K7T              0x029a
#define TSL2561_LUX_B7T              0x0018
#define TSL2561_LUX_M7T              0x0012
#define TSL2561_LUX_K8T              0x029a
#define TSL2561_LUX_B8T              0x0000
#define TSL2561_LUX_M8T              0x0000

typedef uint8_t AddressType;

#define TRANSFER_SIZE 256

uint8_t writeBuffer[sizeof(AddressType) + TRANSFER_SIZE];
uint8_t readBuffer[TRANSFER_SIZE];

// ****Forward Declarations**** //
static int init_i2c(XIic* i2c);
static int tsl2561_write8(XIic* i2c, uint8_t reg, uint8_t value);
static int tsl2561_read8(XIic* i2c, uint8_t reg, uint8_t* value);
static int tsl2561_read16(XIic* i2c, uint8_t reg, uint16_t* value);
static void tsl2561_enable(XIic* i2c);
static void tsl2561_disable(XIic* i2c);

void tsl2561_init(XIic* i2c) {
    int Status;
    uint8_t id;

    if(i2c == NULL) {
        DEBUG_PRINT("TSL2561 I2C instance pointer is NULL\n");
        return;
    }
    
    if(!i2c->IsReady) {
        // device likely not initialized, so initialize it
        Status = init_i2c(i2c);
        if (Status != XST_SUCCESS) {
            DEBUG_PRINT("I2C hardware driver initialization failed\n");
            return;
        }
    }

    // Verify device ID
    Status = tsl2561_read8(i2c, TSL2561_COMMAND_BIT | TSL2561_REGISTER_ID, &id);
    if (Status != XST_SUCCESS) {
        DEBUG_PRINT("Failed to read TSL2561 device ID\n");
        return;
    }
    
    DEBUG_PRINT("TSL2561 ID: 0x%02X\n", id);

    // Power on the device
    tsl2561_enable(i2c);
    
    // Set integration time to 13ms and gain to 1x
    Status = tsl2561_write8(i2c, TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING, 
                           TSL2561_INTEGRATIONTIME_13MS | TSL2561_GAIN_1X);
    if (Status != XST_SUCCESS) {
        DEBUG_PRINT("Failed to configure TSL2561 timing\n");
        return;
    }

    DEBUG_PRINT("TSL2561 initialized successfully\n");
}

uint16_t tsl2561_readChannel(XIic* i2c, tsl2561_channel_t channel) {
    int Status;
    uint16_t value = 0;
    uint8_t reg;

    if(i2c == NULL) {
        DEBUG_PRINT("TSL2561 I2C instance pointer is NULL\n");
        return 0;
    }

    // Enable the device
    tsl2561_enable(i2c);

    // Wait for integration time
    usleep(TSL2561_DELAY_INTTIME_13MS * 1000);

    // Read the appropriate channel
    if (channel == TSL2561_CHANNEL_0) {
        reg = TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN0_LOW;
    } else {
        reg = TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN1_LOW;
    }

    Status = tsl2561_read16(i2c, reg, &value);
    if (Status != XST_SUCCESS) {
        DEBUG_PRINT("Failed to read TSL2561 channel %d\n", channel);
        value = 0;
    }

    // Disable the device to save power
    tsl2561_disable(i2c);

    return value;
}

uint32_t tsl2561_calculateLux(uint16_t ch0, uint16_t ch1) {
    unsigned long chScale;
    unsigned long channel0;
    unsigned long channel1;
    unsigned long ratio1;
    unsigned long ratio;
    unsigned int b, m;
    unsigned long temp;
    uint32_t lux;

    // Use 13ms integration time scaling
    chScale = TSL2561_LUX_CHSCALE_TINT0;

    // Scale for gain (1x, so shift by 4)
    chScale = chScale << 4;

    // Scale the channel values
    channel0 = (ch0 * chScale) >> TSL2561_LUX_CHSCALE;
    channel1 = (ch1 * chScale) >> TSL2561_LUX_CHSCALE;

    // Find the ratio of the channel values (Channel1/Channel0)
    ratio1 = 0;
    if (channel0 != 0) {
        ratio1 = (channel1 << (TSL2561_LUX_RATIOSCALE + 1)) / channel0;
    }

    // Round the ratio value
    ratio = (ratio1 + 1) >> 1;

    // Determine b and m coefficients based on ratio
    if ((ratio >= 0) && (ratio <= TSL2561_LUX_K1T)) {
        b = TSL2561_LUX_B1T;
        m = TSL2561_LUX_M1T;
    } else if (ratio <= TSL2561_LUX_K2T) {
        b = TSL2561_LUX_B2T;
        m = TSL2561_LUX_M2T;
    } else if (ratio <= TSL2561_LUX_K3T) {
        b = TSL2561_LUX_B3T;
        m = TSL2561_LUX_M3T;
    } else if (ratio <= TSL2561_LUX_K4T) {
        b = TSL2561_LUX_B4T;
        m = TSL2561_LUX_M4T;
    } else if (ratio <= TSL2561_LUX_K5T) {
        b = TSL2561_LUX_B5T;
        m = TSL2561_LUX_M5T;
    } else if (ratio <= TSL2561_LUX_K6T) {
        b = TSL2561_LUX_B6T;
        m = TSL2561_LUX_M6T;
    } else if (ratio <= TSL2561_LUX_K7T) {
        b = TSL2561_LUX_B7T;
        m = TSL2561_LUX_M7T;
    } else {
        b = TSL2561_LUX_B8T;
        m = TSL2561_LUX_M8T;
    }

    // Calculate lux
    channel0 = channel0 * b;
    channel1 = channel1 * m;

    temp = 0;
    // Do not allow negative lux value
    if (channel0 > channel1) {
        temp = channel0 - channel1;
    }

    // Round lsb
    temp += (1 << (TSL2561_LUX_LUXSCALE - 1));

    // Strip off fractional portion
    lux = temp >> TSL2561_LUX_LUXSCALE;

    return lux;
}

// ****Private Helper Functions**** //
static int init_i2c(XIic* i2c) {
    int status;
    XIic_Config* configPtr;

    configPtr = XIic_LookupConfig(XPAR_XIIC_0_BASEADDR);
    if (configPtr == NULL) {
        return XST_FAILURE;
    }

    status = XIic_CfgInitialize(i2c, configPtr, configPtr->BaseAddress);
    if (status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    return XST_SUCCESS;
}

static int tsl2561_write8(XIic* i2c, uint8_t reg, uint8_t value) {
    int Status;
    
    writeBuffer[0] = reg;
    writeBuffer[1] = value;
    
    Status = XIic_Send(i2c->BaseAddress, TSL2561_ADDR, writeBuffer, 2, XIIC_STOP);
    if (Status != 2) {
        return XST_FAILURE;
    }
    
    return XST_SUCCESS;
}

static int tsl2561_read8(XIic* i2c, uint8_t reg, uint8_t* value) {
    int Status;
    
    // Send register address
    Status = XIic_Send(i2c->BaseAddress, TSL2561_ADDR, &reg, 1, XIIC_REPEATED_START);
    if (Status != 1) {
        return XST_FAILURE;
    }
    
    // Read the value
    Status = XIic_Recv(i2c->BaseAddress, TSL2561_ADDR, value, 1, XIIC_STOP);
    if (Status != 1) {
        return XST_FAILURE;
    }
    
    return XST_SUCCESS;
}

static int tsl2561_read16(XIic* i2c, uint8_t reg, uint16_t* value) {
    int Status;
    uint8_t data[2];
    
    // Send register address
    Status = XIic_Send(i2c->BaseAddress, TSL2561_ADDR, &reg, 1, XIIC_REPEATED_START);
    if (Status != 1) {
        return XST_FAILURE;
    }
    
    // Read 2 bytes (little-endian)
    Status = XIic_Recv(i2c->BaseAddress, TSL2561_ADDR, data, 2, XIIC_STOP);
    if (Status != 2) {
        return XST_FAILURE;
    }
    
    // Combine bytes (LSB first)
    *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    
    return XST_SUCCESS;
}

static void tsl2561_enable(XIic* i2c) {
    tsl2561_write8(i2c, TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL, 
                   TSL2561_CONTROL_POWERON);
}

static void tsl2561_disable(XIic* i2c) {
    tsl2561_write8(i2c, TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL, 
                   TSL2561_CONTROL_POWEROFF);
}