cmake_minimum_required(VERSION 3.25)

# Project definition
project(CounterStrikeCpp
    VERSION 0.1.0
    DESCRIPTION "Modern C++ game engine with GoldSrc movement and networking"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Options
option(CSCPP_BUILD_TESTS "Build unit tests" ON)
option(CSCPP_BUILD_TOOLS "Build development tools" ON)
option(CSCPP_BUILD_SERVER "Build dedicated server" ON)
option(CSCPP_BUILD_CLIENT "Build game client" ON)
option(CSCPP_ENABLE_PROFILING "Enable Tracy profiling" OFF)
option(CSCPP_GOLDSCR_PARITY "Enable exact GoldSrc float parity mode" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# =============================================================================
# Dependencies (via vcpkg)
# =============================================================================

# Check if vcpkg toolchain is provided
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
else()
    message(WARNING "vcpkg toolchain not specified. Attempting to find packages in system paths...")
    message(STATUS "To use vcpkg, run:")
    message(STATUS "  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake")
    message(STATUS "Common vcpkg locations:")
    message(STATUS "  C:\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake")
    message(STATUS "  %USERPROFILE%\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake")
endif()

find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(unofficial-enet CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Optional dependencies
find_package(OpenAL CONFIG)
find_package(glad CONFIG)

# tinygltf is header-only, find in vcpkg or system
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
if(TINYGLTF_INCLUDE_DIRS)
    set(TINYGLTF_FOUND TRUE)
endif()

# =============================================================================
# Core Library
# =============================================================================

add_library(cscpp_core STATIC
    # Platform
    src/core/platform/window.cpp
    src/core/platform/input.cpp
    
    # Logging
    src/core/logging/logger.cpp
)

target_include_directories(cscpp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cscpp_core PUBLIC
    SDL2::SDL2
    glm::glm
    spdlog::spdlog
)

if(glad_FOUND)
    target_link_libraries(cscpp_core PUBLIC glad::glad)
endif()

# =============================================================================
# ECS Library
# =============================================================================

add_library(cscpp_ecs STATIC
    src/ecs/world/world.cpp
)

target_link_libraries(cscpp_ecs PUBLIC
    cscpp_core
    EnTT::EnTT
)

# =============================================================================
# Movement Library (GoldSrc pm_shared port)
# =============================================================================

add_library(cscpp_movement STATIC
    # PM_Shared port
    src/movement/pm_shared/pm_shared.cpp
)

target_link_libraries(cscpp_movement PUBLIC
    cscpp_core
    cscpp_ecs
)

target_compile_definitions(cscpp_movement PUBLIC
    $<$<BOOL:${CSCPP_GOLDSCR_PARITY}>:CSCPP_GOLDSRC_PARITY>
)

# =============================================================================
# Networking Library
# =============================================================================

add_library(cscpp_network STATIC
    src/network/network_stub.cpp
)

target_link_libraries(cscpp_network PUBLIC
    cscpp_core
    cscpp_ecs
    cscpp_movement
    unofficial::enet::enet
)

# =============================================================================
# Renderer Library
# =============================================================================

add_library(cscpp_renderer STATIC
    src/renderer/renderer_stub.cpp
    src/renderer/backend/gl_shader.cpp
    src/renderer/backend/gl_mesh.cpp
    src/renderer/simple_renderer.cpp
)

target_link_libraries(cscpp_renderer PUBLIC
    cscpp_core
    cscpp_ecs
    OpenGL::GL
    imgui::imgui
)

# =============================================================================
# Assets Library
# =============================================================================

add_library(cscpp_assets STATIC
    src/assets/assets_stub.cpp
    src/assets/bsp/simple_bsp_loader.cpp
    src/assets/gltf/simple_gltf_loader.cpp
)

target_link_libraries(cscpp_assets PUBLIC
    cscpp_core
    cscpp_renderer
)

if(TINYGLTF_FOUND)
    target_include_directories(cscpp_assets PRIVATE ${TINYGLTF_INCLUDE_DIRS})
endif()

# =============================================================================
# Gameplay Library
# =============================================================================

add_library(cscpp_gameplay STATIC
    src/gameplay/gameplay_stub.cpp
)

target_link_libraries(cscpp_gameplay PUBLIC
    cscpp_core
    cscpp_ecs
    cscpp_movement
    cscpp_network
)

# =============================================================================
# Audio Library
# =============================================================================

add_library(cscpp_audio STATIC
    src/audio/audio_stub.cpp
)

target_link_libraries(cscpp_audio PUBLIC
    cscpp_core
)

if(OpenAL_FOUND)
    target_link_libraries(cscpp_audio PRIVATE OpenAL::OpenAL)
    target_compile_definitions(cscpp_audio PRIVATE CSCPP_HAS_OPENAL)
endif()

# =============================================================================
# UI Library
# =============================================================================

add_library(cscpp_ui STATIC
    src/ui/ui_stub.cpp
)

target_link_libraries(cscpp_ui PUBLIC
    cscpp_core
    cscpp_renderer
    imgui::imgui
)

# =============================================================================
# Helper function to copy DLLs to output directory
# =============================================================================

function(copy_dlls_to_output target_name)
    if(WIN32)
        # Copy SDL2 DLL from vcpkg_installed directory
        # Uses generator expressions to handle both Debug and Release configurations
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin/SDL2d.dll,${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/SDL2.dll>"
                "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
            COMMENT "Copying SDL2 DLL to output directory"
        )
    endif()
endfunction()

# =============================================================================
# Executables
# =============================================================================

# Game Client
if(CSCPP_BUILD_CLIENT)
    add_executable(cscpp_client
        src/client_main.cpp
    )
    
    target_link_libraries(cscpp_client PRIVATE
        cscpp_core
        cscpp_ecs
        cscpp_movement
        cscpp_network
        cscpp_renderer
        cscpp_assets
        cscpp_gameplay
        cscpp_audio
        cscpp_ui
    )
    
    if(WIN32)
        target_link_libraries(cscpp_client PRIVATE SDL2::SDL2main)
    endif()
    
    # Copy DLLs to output directory
    copy_dlls_to_output(cscpp_client)
    
    # Copy assets to build directory
    add_custom_command(TARGET cscpp_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
        COMMENT "Copying assets to build directory"
    )
endif()

# Dedicated Server
if(CSCPP_BUILD_SERVER)
    add_executable(cscpp_server
        src/server_main.cpp
    )
    
    target_link_libraries(cscpp_server PRIVATE
        cscpp_core
        cscpp_ecs
        cscpp_movement
        cscpp_network
        cscpp_gameplay
    )
endif()

# =============================================================================
# Tests
# =============================================================================

if(CSCPP_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    
    # Tests will be added when test files are created
    # add_executable(test_movement ...)
    # add_executable(test_network ...)
endif()

# =============================================================================
# Tools
# =============================================================================

if(CSCPP_BUILD_TOOLS)
    # Tools will be added when tool files are created
    # add_executable(replay_tool ...)
    # add_executable(asset_compiler ...)
endif()

# =============================================================================
# Install
# =============================================================================

install(TARGETS cscpp_client cscpp_server
    RUNTIME DESTINATION bin
)

install(DIRECTORY assets/
    DESTINATION share/cscpp/assets
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "Counter-Strike C++ Engine Configuration:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests:      ${CSCPP_BUILD_TESTS}")
message(STATUS "  Build tools:      ${CSCPP_BUILD_TOOLS}")
message(STATUS "  Build server:     ${CSCPP_BUILD_SERVER}")
message(STATUS "  Build client:     ${CSCPP_BUILD_CLIENT}")
message(STATUS "  GoldSrc parity:   ${CSCPP_GOLDSCR_PARITY}")
message(STATUS "  Tracy profiling:  ${CSCPP_ENABLE_PROFILING}")
message(STATUS "")

