cmake_minimum_required(VERSION 3.20)
project(raptor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/libraries)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    raptor/src/*.cpp
    raptor/src/*.c
)

add_executable(raptor ${SOURCES})
set_target_properties(raptor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out"
)

target_include_directories(raptor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/raptor/include)
target_include_directories(raptor PRIVATE ${LIB_DIR}/include)
