cmake_minimum_required(VERSION 3.10)
project(interdit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Verify X11 installation
find_package(X11 REQUIRED)
if(NOT X11_FOUND)
    message(FATAL_ERROR "X11 development libraries not found! Install them with:\n  sudo apt-get install libx11-dev")
endif()

add_executable(interdit
    src/main.cpp
    src/Volume.cpp
    src/Viewer.cpp
)

target_include_directories(interdit PRIVATE 
    include
    ${X11_INCLUDE_DIR}
)

target_link_libraries(interdit PRIVATE 
    ${X11_LIBRARIES}
    m
    pthread
)
