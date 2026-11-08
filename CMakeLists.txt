cmake_minimum_required(VERSION 3.20)
project(ModrionNetSpectre LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE HOST_ARCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(HOST_ARCH STREQUAL "arm64" OR HOST_ARCH STREQUAL "x86_64")
        set(CMAKE_OSX_ARCHITECTURES "${HOST_ARCH}" CACHE STRING "macOS architecture" FORCE)
    endif()
endif()

add_executable(Netspro src/main.cpp src/downloader.cpp src/download_service.cpp)
find_package(Threads REQUIRED)
target_link_libraries(Netspro PRIVATE Threads::Threads)
target_include_directories(Netspro PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set_target_properties(Netspro PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
