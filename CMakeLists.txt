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

add_executable(Netspro src/main.cpp src/downloader.cpp src/download_service.cpp src/catalog_sources.cpp)
find_package(Threads REQUIRED)
target_link_libraries(Netspro PRIVATE Threads::Threads)
target_include_directories(Netspro PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(NETSPRO_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Executable output directory")
set_target_properties(Netspro PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${NETSPRO_OUTPUT_DIRECTORY}"
)
foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(Netspro PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${configuration}" "${NETSPRO_OUTPUT_DIRECTORY}")
endforeach()
if(WIN32)
    target_compile_definitions(Netspro PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
# Keep Windows installations independent of a separately installed MSVC runtime.
if(MSVC)
    set_property(TARGET Netspro PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
