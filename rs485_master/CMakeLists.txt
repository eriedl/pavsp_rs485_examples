cmake_minimum_required(VERSION 2.8.4)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/ArduinoToolchain.cmake)
set(PROJECT_NAME rs485_master)
project(${PROJECT_NAME})

#/Users/eriedl/Documents/Arduino/libraries/Timer
#Event.h Event.cpp Timer.h Timer.cpp

include_directories(/Users/eriedl/Documents/Arduino/libraries/Timer /Users/eriedl/Documents/Arduino/libraries/afLib)
#set(SOURCE_FILES Event.cpp Timer.cpp)

set(${CMAKE_PROJECT_NAME}_BOARD mega)
set(mega.build.mcu atmega2560)
set(mega.upload.protocol wiring)
set(mega.upload.speed 115200)

set(${CMAKE_PROJECT_NAME}_SKETCH rs485_master.ino)
generate_arduino_firmware(${CMAKE_PROJECT_NAME})