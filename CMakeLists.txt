
cmake_minimum_required(VERSION 3.25)

project(IndustrialFrontier
    VERSION 0.1.0
    DESCRIPTION "Industrial Frontier - Mining and Industrial Automation Simulation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(IndustrialFrontier
    src/main.cpp
    src/core/Application.cpp
)

target_include_directories(IndustrialFrontier
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(IndustrialFrontier PRIVATE
        /W4
        /permissive-
        /Zc:preprocessor
        /EHsc
    )
endif()

message(STATUS "Industrial Frontier configuration")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
