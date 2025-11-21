/**
 * @file tsl2561.c
 * @brief TSL2561 Light Sensor Driver for Xilinx IIC
 * 
 * This driver provides minimal functionality for the TSL2561 light sensor,
 * including initialization, channel reading, and lux calculation.
 * 
 * @note Implementation adapted from Adafruit TSL2561 driver:
 * https://github.com/adafruit/Adafruit_TSL2561/blob/master/Adafruit_TSL2561_U.cpp
 * 
 * 
 * @note Uses Xilinx IIC driver API for I2C communication
 * @note Configured for 13ms integration time and 1x gain
 */

#pragma once 


typedef enum {
    TSL2561_CHANNEL_0 = 0,
    TSL2561_CHANNEL_1 = 1
} tsl2561_channel_t;

/****************** Include Files ********************/
#include "xstatus.h"
#include "xiic.h"
#include "xparameters.h"

//Requried methods for project

void tsl2561_init(XIic* i2c);

uint16_t tsl2561_readChannel(XIic* i2c, tsl2561_channel_t channel);

uint32_t tsl2561_calculateLux(uint16_t ch0, uint16_t ch1);
