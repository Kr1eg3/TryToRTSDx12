cmake_minimum_required(VERSION 3.20)

project(CppRTSDx12 
    VERSION 1.0.0
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories - Visual Studio uses configuration-specific subdirectories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# For multi-config generators like Visual Studio
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG})
endforeach()

# Ensure we're on Windows for DirectX 12
if(NOT WIN32)
    message(FATAL_ERROR "This project requires Windows for DirectX 12 support")
endif()

# Configuration-specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG DEBUG_BUILD=1)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd")
else()
    add_compile_definitions(NDEBUG DEBUG_BUILD=0)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /MD")
endif()

# Compiler-specific settings for MSVC
if(MSVC)
    add_compile_options(
        /W3                     # Warning level 3
        /permissive-           # Strict conformance mode
        /MP                     # Multi-processor compilation
        /fp:fast               # Fast floating point model
        /std:c++latest         # Latest C++ standard
    )
    
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00    # Windows 10
    )
endif()

# Find DirectX 12 components
find_path(DirectX_INCLUDE_DIR
    NAMES d3d12.h
    PATHS
        "C:/Program Files (x86)/Windows Kits/10/Include"
        "$ENV{WindowsSdkDir}Include"
    PATH_SUFFIXES
        "10.0.26100.0/um"
        "10.0.22621.0/um"
        "10.0.19041.0/um"
        "um"
    REQUIRED
)

# DirectX 12 libraries
set(DirectX_LIBRARIES
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)

# Add ThirdParty directory for external dependencies
add_subdirectory(ThirdParty)

# Core engine libraries
add_subdirectory(Source/Core)
add_subdirectory(Source/Platform)
add_subdirectory(Source/Rendering)

# Main executable
add_executable(${PROJECT_NAME} WIN32
    WinMain.cpp
)

# Link everything together
target_link_libraries(${PROJECT_NAME} PRIVATE
    Core
    Platform
    Rendering
    ${DirectX_LIBRARIES}
    assimp
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    Source
    ${DirectX_INCLUDE_DIR}
)

# Copy assets to output directories for each configuration
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    # Copy assets
    if(EXISTS "${CMAKE_SOURCE_DIR}/Assets")
        file(COPY "${CMAKE_SOURCE_DIR}/Assets" DESTINATION "${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}")
    endif()
endforeach()

# Set startup project for Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

# Set working directory for debugging
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/$(Configuration)"
)

# Create custom target to copy DLLs
add_custom_target(CopyDependencies
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:assimp>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying required DLLs to output directory"
    DEPENDS assimp
)

# Make sure DLLs are copied before the main executable can run
add_dependencies(${PROJECT_NAME} CopyDependencies)

# Also add post-build step as backup
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:assimp>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Post-build: Ensuring assimp DLL is in output directory"
)