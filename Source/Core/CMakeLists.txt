# Core library - main engine systems
add_library(Core STATIC
    # Application
    Application/Application.cpp
    Application/Application.h
    Application/Timer.cpp
    Application/Timer.h
    
    # Entity Component System
    Entity/Entity.cpp
    Entity/Entity.h
    Entity/Component.h
    Entity/MeshComponent.cpp
    Entity/MeshComponent.h
    Entity/TransformComponent.cpp
    Entity/TransformComponent.h
    
    # Scene Management
    Scene/Scene.cpp
    Scene/Scene.h
    
    # Utilities
    Utilities/Types.h
    Utilities/TextureLoader.h
    Utilities/TextureLoader.cpp
    Window/Window.h
)

target_include_directories(Core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Source
)

# Link DirectXMath (header-only)
target_link_libraries(Core PUBLIC
    Platform
)

target_compile_definitions(Core PUBLIC
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)