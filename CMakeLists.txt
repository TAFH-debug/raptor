cmake_minimum_required(VERSION 3.20)
project(raptor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    raptor/src/*.cpp
    raptor/src/*.c
)


include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2      # pick any release tag
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

find_package(Threads REQUIRED)
target_link_libraries(asio INTERFACE Threads::Threads)

if(WIN32)
    target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(asio INTERFACE ws2_32 mswsock)
endif()

add_library(raptor STATIC ${SOURCES})
target_include_directories(raptor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/raptor/include)
target_link_libraries(raptor PUBLIC asio)


add_executable(raptor_server raptor/example/server.cpp)
set_target_properties(raptor_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out"
)
target_link_libraries(raptor_server PRIVATE raptor)

add_executable(raptor_client raptor/example/client.cpp)
set_target_properties(raptor_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/out"
)
target_link_libraries(raptor_client PRIVATE raptor)
