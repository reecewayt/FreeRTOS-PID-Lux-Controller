/**
 * @file tsl2561.h
 * @brief Header file for the TSL2561 light sensor driver.
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
