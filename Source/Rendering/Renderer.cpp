#include "Renderer.h"

#if 0
// Include platform-specific renderer
#ifdef _WIN32
    #include "Dx12/DX12Renderer.h"
#endif

// Factory method implementation
UniquePtr<Renderer> Renderer::Create() {
#ifdef _WIN32
    return std::make_unique<DX12Renderer>();
#else
    #error "Platform not supported"
    return nullptr;
#endif
}
#endif