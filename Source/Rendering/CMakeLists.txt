# Rendering library - DirectX 12 rendering code
add_library(Rendering STATIC
    # Base rendering
    Renderer.cpp
    Renderer.h
    Camera.cpp
    Camera.h
    Mesh.cpp
    Mesh.h
    
    # DirectX 12 specific
    Dx12/DX12Renderer.cpp
    Dx12/DX12Renderer.h
    
    # RHI (Render Hardware Interface)
    RHI/IRHIContext.h
    RHI/RHITypes.h
    RHI/DX12RHIContext.h
    RHI/DX12RHIContext.cpp
    
    # Bindable objects
    Bindable/IBindable.h
    Bindable/VertexBuffer.h
    Bindable/IndexBuffer.h
    Bindable/IndexBuffer.cpp
    Bindable/ConstantBuffer.h
    Bindable/BindableBase.h
    Bindable/Texture.h
    Bindable/Texture.cpp
    Bindable/Sampler.h
    Bindable/Sampler.cpp
    
    # Material system
    Material.h
    Material.cpp
)

target_include_directories(Rendering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${DirectX_INCLUDE_DIR}
)

# Link DirectX 12 and related libraries
target_link_libraries(Rendering PUBLIC
    Core
    Platform
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)

target_compile_definitions(Rendering PUBLIC
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)