#pragma once

#include "../Renderer.h"
#include "../../Platform/Windows/WindowsPlatform.h"

class Window;

class DX12Renderer : public Renderer {
public:
    DX12Renderer();
    virtual ~DX12Renderer();

    // Renderer interface implementation
    bool Initialize(Window* window, const RendererConfig& config) override;
    void Shutdown() override;

    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;

    void Clear(const ClearValues& clearValues) override;
    void SetViewport(const ViewportDesc& viewport) override;

    void WaitForGpu() override;
    void Resize(uint32 width, uint32 height) override;

    void SetDebugName(void* resource, const String& name) override;
    uint64 GetGpuMemoryUsage() const override;
    uint32 GetCurrentFrameIndex() const override { return m_currentFrameIndex; }

    // Resource creation helpers for meshes and shaders
    bool CreateBuffer(uint64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState,
                     ComPtr<ID3D12Resource>& buffer, const void* data = nullptr,
                     ComPtr<ID3D12Resource>* uploadBuffer = nullptr);

    bool CreateVertexBuffer(const void* data, uint64 size, ComPtr<ID3D12Resource>& vertexBuffer,
                           ComPtr<ID3D12Resource>& uploadBuffer, D3D12_VERTEX_BUFFER_VIEW& bufferView);

    bool CreateIndexBuffer(const void* data, uint64 size, ComPtr<ID3D12Resource>& indexBuffer,
                          ComPtr<ID3D12Resource>& uploadBuffer, D3D12_INDEX_BUFFER_VIEW& bufferView);

    bool CreateConstantBuffer(uint64 size, ComPtr<ID3D12Resource>& constantBuffer, void** mappedData = nullptr);

    // Shader compilation
    bool CompileShader(const String& source, const String& entryPoint, const String& target,
                      ComPtr<ID3DBlob>& shaderBlob);

    // Descriptor heap access
    bool CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors,
                             D3D12_DESCRIPTOR_HEAP_FLAGS flags, ComPtr<ID3D12DescriptorHeap>& heap);

    uint32 GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

    // Device access for external classes
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }

private:
    // Core D3D12 objects
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain();
    bool CreateDescriptorHeaps();
    bool CreateRenderTargets();
    bool CreateDepthStencil();
    bool CreateCommandAllocators();
    bool CreateCommandList();
    bool CreateSynchronization();

    // Resource management
    void CreateRtvDescriptorHeap();
    void CreateDsvDescriptorHeap();

    // Frame synchronization
    void MoveToNextFrame();
    void WaitForFrame(uint32 frameIndex);

    // Debug helpers
    void EnableDebugLayer();
    void SetupDebugDevice();

    // Helper for resource uploads
    void ExecuteCommandListAndWait();

private:
    // Window reference
    Window* m_window = nullptr;
    HWND m_hwnd = nullptr;
    uint32 m_windowWidth = 0;
    uint32 m_windowHeight = 0;

    // Configuration
    RendererConfig m_config;
    uint32 m_backBufferCount = 2;
    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D32_FLOAT;

    // Core D3D12 objects
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Descriptor heaps
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    uint32 m_rtvDescriptorSize = 0;
    uint32 m_dsvDescriptorSize = 0;

    // Render targets
    Vector<ComPtr<ID3D12Resource>> m_renderTargets;
    ComPtr<ID3D12Resource> m_depthStencilBuffer;

    // Command allocators (one per frame)
    Vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;

    // Synchronization
    ComPtr<ID3D12Fence> m_fence;
    Vector<uint64> m_fenceValues;
    uint64 m_currentFenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    // Frame tracking
    uint32 m_currentFrameIndex = 0;
    uint32 m_currentBackBufferIndex = 0;
    bool m_isInitialized = false;

    // Debug
    ComPtr<ID3D12Debug> m_debugController;
    ComPtr<ID3D12DebugDevice> m_debugDevice;

    DECLARE_NON_COPYABLE(DX12Renderer);
};