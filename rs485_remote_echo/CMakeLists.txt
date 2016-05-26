cmake_minimum_required(VERSION 2.8.4)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/ArduinoToolchain.cmake)
set(PROJECT_NAME RS485_RemoteEcho)
project(${PROJECT_NAME})

include_directories("/Applications/Arduino.app/Contents/Java/hardware/arduino/avr/libraries/SoftwareSerial")

#set(SOURCE_FILES,
#        /Applications/Arduino.app/Contents/Java/hardware/arduino/avr/libraries/SoftwareSerial/SoftwareSerial.cpp
#        /Applications/Arduino.app/Contents/Java/hardware/arduino/avr/libraries/SoftwareSerial/SoftwareSerial.h
#        )

set(${CMAKE_PROJECT_NAME}_SKETCH RS485_RemoteEcho.ino)
generate_arduino_firmware(${CMAKE_PROJECT_NAME})
