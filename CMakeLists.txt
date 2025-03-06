cmake_minimum_required(VERSION 3.15)
project(interdit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find fltk-config script
find_program(FLTK_CONFIG fltk-config
    HINTS /usr/local/bin /usr/bin /opt/local/bin
    DOC "FLTK configuration script"
)

if (NOT FLTK_CONFIG)
    message(FATAL_ERROR "fltk-config not found! Install FLTK development package")
endif ()

# Create executable
set(INTERDIT_EXECUTABLE_SOURCES
    ../src/interdit.cpp
)

set(INTERDIT_EXECUTABLE_NAME interdit)

add_custom_target(${INTERDIT_EXECUTABLE_NAME})

add_custom_command(
    TARGET ${INTERDIT_EXECUTABLE_NAME}
    COMMAND ${FLTK_CONFIG} --compile ${INTERDIT_EXECUTABLE_SOURCES}
    DEPENDS ${INTERDIT_EXECUTABLE_SOURCES} ../include/CLI11.hpp  # List all headers
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

