
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
    src/core/GameFlow.cpp

    src/player/PlayerController.cpp

    src/renderer/Renderer.cpp
    src/renderer/Shader.cpp

    src/ui/UIRenderer.cpp
    src/ui/MainMenu.cpp

    src/world/TerrainGenerator.cpp
    src/world/TerrainChunk.cpp
    src/world/TerrainWorld.cpp
    src/world/TerrainGenerationWorker.cpp
    src/world/VoxelTerrain.cpp
    src/world/VoxelMesher.cpp
)

target_include_directories(IndustrialFrontier
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(IndustrialFrontier
    PRIVATE
        SDL3::SDL3
        OpenGL::GL
)

if(MSVC)
    target_compile_options(IndustrialFrontier
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(IndustrialFrontier
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_custom_command(
    TARGET IndustrialFrontier
    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:IndustrialFrontier>/assets"

    COMMENT "Copying Industrial Frontier assets"
)

if(WIN32)
    add_custom_command(
        TARGET IndustrialFrontier
        POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:IndustrialFrontier>"

        COMMENT "Copying SDL3 runtime"
    )
endif()
