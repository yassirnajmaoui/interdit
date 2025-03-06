cmake_minimum_required(VERSION 3.15)
project(interdit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Verify FLTK installation
find_package(FLTK CONFIG REQUIRED NO_MODULE)
find_package(X11 REQUIRED)

add_executable(interdit
    src/main.cpp
)

target_include_directories(interdit PRIVATE 
    include
    ${FLTK_INCLUDE_DIR}
    ${X11_INCLUDE_DIR}
)

target_link_libraries(interdit PRIVATE
    fltk
    ${FLTK_IMAGES_LIBRARY}
    ${X11_LIBRARIES}
    ${XFIXES_LIBRARY}
    ${XRENDER_LIBRARY}
    ${XCURSOR_LIBRARY}
    ${XINERAMA_LIBRARY}
    pthread
)
