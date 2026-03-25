# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# (Not part of the boilerplate)
# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET "esp32s3")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DxSemaphoreHandle=SemaphoreHandle_t -DxTimerHandle=TimerHandle_t -DxQueueHandle=QueueHandle_t -DxTaskHandle=TaskHandle_t -DportTickType=TickType_t -DportTICK_RATE_MS=portTICK_PERIOD_MS")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DxSemaphoreHandle=SemaphoreHandle_t -DxTimerHandle=TimerHandle_t -DxQueueHandle=QueueHandle_t -DxTaskHandle=TaskHandle_t -DportTickType=TickType_t -DportTICK_RATE_MS=portTICK_PERIOD_MS")

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(videodoorbell_esp32)
