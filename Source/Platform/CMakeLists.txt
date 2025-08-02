# Platform library - Windows-specific code
add_library(Platform STATIC
    Windows/Win32Window.cpp
    Windows/Win32Window.h
    Windows/WindowsPlatform.cpp
    Windows/WindowsPlatform.h
)

target_include_directories(Platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Source
)

# Windows-specific libraries
target_link_libraries(Platform PUBLIC
    user32.lib
    kernel32.lib
    gdi32.lib
    shell32.lib
)

target_compile_definitions(Platform PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)