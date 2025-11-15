#pragma once

/**
 * @file logging.h
 * @brief A simple, tag-based debug logging utility for Xilinx projects.
 *
 * This header provides a simple `DEBUG_PRINT` macro that wraps `xil_printf`.
 * It allows each .c file to define its own `LOG_TAG` for categorized output,
 * making it easier to identify the source of a log message.
 *
 * Logging can be globally enabled or disabled by defining/undefining
 * the `DEBUG_1` macro in the config.h file.
 */

#include "xil_printf.h"
#include "config.h"

// --- MACRO DEFINITIONS ---

// 1. Provide a default tag if one isn't defined by the including .c file
#ifndef LOG_TAG
#define LOG_TAG "APP" // Default tag
#endif

#ifdef DEBUG_1
    // 2. LOGGING IS ON: Prepend the LOG_TAG to the user's format string.
    //    The compiler automatically concatenates the "[%s] " string with the 'fmt' string.
    #define DEBUG_PRINT(fmt, ...) xil_printf("[%s] " fmt, LOG_TAG, ##__VA_ARGS__)
#else
    // 3. LOGGING IS OFF: Define the macro as a "no-op" (no operation).
    //    The (void)0 cast prevents compiler warnings about unused statements.
    #define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

// --- END MACRO DEFINITIONS ---

/*
 * -----------------------------------------------------------------------------
 * USAGE GUIDE
 * -----------------------------------------------------------------------------
 *
 * 1. ENABLE/DISABLE LOGGING:
 * To **enable** logging, define `DEBUG_1` 
 * (e.g., add `-DDEBUG_1` to your CFLAGS) or simply change config.h file.
 * If `DEBUG_1` is not defined, all `DEBUG_PRINT` calls will be compiled
 * out and will not consume any code space or CPU time.
 *
 * 2. HOW TO USE IN A .C FILE:
 * In your .c file, define `LOG_TAG` *before* you include this "logging.h"
 * header. This sets the tag for all `DEBUG_PRINT` calls within that file.
 *
 * EXAMPLE (in spi_driver.c):
 * -------------------------------------------------
 * #include "my_includes.h"
 *
 * // Define the tag for this specific file
 * #define LOG_TAG "SPI_DRV"
 * #include "logging.h" // Include this header *after* defining the tag
 *
 * void spi_init() {
 * DEBUG_PRINT("SPI Initialized.\n");
 * DEBUG_PRINT("SPI clock set to %d MHz\n", 50);
 * }
 * -------------------------------------------------
 *
 * EXAMPLE (in main.c):
 * -------------------------------------------------
 * // Define a different tag for main
 * #define LOG_TAG "MAIN"
 * #include "logging.h"
 *
 * int main() {
 * DEBUG_PRINT("Application started.\n");
 * spi_init();
 * return 0;
 * }
 * -------------------------------------------------
 *
 * 3. EXAMPLE OUTPUT (if DEBUG_1 is defined):
 *
 * [MAIN] Application started.
 * [SPI_DRV] SPI Initialized.
 * [SPI_DRV] SPI clock set to 50 MHz
 *
 */