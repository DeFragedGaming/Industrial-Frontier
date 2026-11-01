cmake_minimum_required(VERSION 3.25)

project(IndustrialFrontier
    VERSION 0.1.0
    DESCRIPTION "Industrial Frontier - Mining and Industrial Automation Simulation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.zip
    URL_HASH SHA256=9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(SDL3)

find_package(OpenGL REQUIRED)

add_executable(IndustrialFrontier
    src/main.cpp
    src/core/Application.cpp
    src/renderer/Renderer.cpp
)

target_include_directories(IndustrialFrontier
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(IndustrialFrontier
    PRIVATE
        SDL3::SDL3
        OpenGL::GL
)

target_compile_definitions(IndustrialFrontier
    PRIVATE
        SDL_MAIN_HANDLED
)

if(MSVC)
    target_compile_options(IndustrialFrontier PRIVATE
        /W4
        /permissive-
        /Zc:preprocessor
        /EHsc
    )
endif()

if(WIN32)
    add_custom_command(TARGET IndustrialFrontier POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:IndustrialFrontier>
    )
endif()

message(STATUS "Industrial Frontier configuration")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "SDL3 Version: 3.4.18")
message(STATUS "OpenGL: Enabled")
message(STATUS "3D Renderer: Enabled")