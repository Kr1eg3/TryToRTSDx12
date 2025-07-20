#pragma once

#include "../Core/Utilities/Types.h"
#include <DirectXMath.h>

// Forward declarations
class Window;
struct RenderCommand;

// Renderer configuration
struct RendererConfig {
    bool enableDebugLayer = DEBUG_BUILD;
    bool enableGpuValidation = DEBUG_BUILD;
    bool enableBreakOnError = DEBUG_BUILD;
    uint32 backBufferCount = 2;
    bool vsyncEnabled = true;
    uint32 maxFramesInFlight = 2;
    uint64 gpuMemoryBudgetMB = 512;
};

// Clear values
struct ClearValues {
    DirectX::XMFLOAT4 color = { 0.2f, 0.3f, 0.4f, 1.0f };
    float depth = 1.0f;
    uint8 stencil = 0;
};

// Viewport description
struct ViewportDesc {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

// Abstract renderer interface
class Renderer {
public:
    virtual ~Renderer() = default;

    // Lifecycle
    virtual bool Initialize(Window* window, const RendererConfig& config) = 0;
    virtual void Shutdown() = 0;

    // Frame operations
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;

    // Rendering commands
    virtual void Clear(const ClearValues& clearValues) = 0;
    virtual void SetViewport(const ViewportDesc& viewport) = 0;

    // Resource management
    virtual void WaitForGpu() = 0;
    virtual void Resize(uint32 width, uint32 height) = 0;

    // Debug and profiling
    virtual void SetDebugName(void* resource, const String& name) = 0;
    virtual uint64 GetGpuMemoryUsage() const = 0;
    virtual uint32 GetCurrentFrameIndex() const = 0;

    // Factory method
    static UniquePtr<Renderer> Create();

protected:
    Renderer() = default;
    DECLARE_NON_COPYABLE(Renderer);
};