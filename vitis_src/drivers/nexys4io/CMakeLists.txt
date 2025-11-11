cmake_minimum_required(VERSION 3.16)

add_library(nexys4io STATIC
    src/nexys4io.c
    src/nexys4io_selftest.c
    include/nexys4io.h
    include/nexys4IO_l.h
)

target_include_directories(nexys4io
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_property(TARGET nexys4io PROPERTY C_STANDARD 99)
