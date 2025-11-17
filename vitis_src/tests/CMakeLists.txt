cmake_minimum_required(VERSION 3.10)

# Project Name
project(HostRunnableTests C)

# 1. CONFIGURATION
# ----------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Define where the embedded source code lives relative to this test folder
# Your structure: tests/ -> ../pid_controller/src/
set(EMBEDDED_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../pid_controller/src")

# Include the embedded source directory (so #include "fixed_point.h" works)
include_directories(${EMBEDDED_SRC_DIR})


# 2. DEFINE SHARED SOURCES
# ------------------------
# List all files from your embedded project that need to be compiled for tests
set(SHARED_SRCS
    ${EMBEDDED_SRC_DIR}/fptc.h
    # Add more source files here later, e.g.:
    # ${EMBEDDED_SRC_DIR}/pid.c
)


# 3. EXECUTABLES (TESTS)
# ----------------------

# Test Suite 1: Fixed Point Logic
add_executable(test_fixed_point 
    fp_lib_tests.c 
    ${SHARED_SRCS}
)

# Example of extensibility: 
# To add a PID test later, just create pid_test.c and uncomment below:
# add_executable(test_pid 
#     pid_test.c 
#     ${SHARED_SRCS}
# )