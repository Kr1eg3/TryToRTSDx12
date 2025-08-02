#pragma once

#include "../Renderer.h"
#include "../../Platform/Windows/WindowsPlatform.h"
#include <DirectXMath.h>

// Constant buffer structures
struct ModelConstants {
    DirectX::XMMATRIX modelMatrix;
    DirectX::XMMATRIX normalMatrix;
};

struct ViewConstants {
    DirectX::XMMATRIX viewMatrix;
    DirectX::XMMATRIX projectionMatrix;
    DirectX::XMMATRIX viewProjectionMatrix;
    DirectX::XMFLOAT3 cameraPosition;
    float padding;
};

struct LightConstants {
    DirectX::XMFLOAT3 lightPosition;
    float lightIntensity;
    DirectX::XMFLOAT3 lightColor;
    float padding;
};

struct MaterialConstants {
    DirectX::XMFLOAT3 baseColor;
    float metallic;
    float roughness;
    float padding[3];
};

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

    bool CopyUploadToDefaultBuffer(ComPtr<ID3D12Resource>& defaultBuffer, 
                                  ComPtr<ID3D12Resource>& uploadBuffer, 
                                  uint64 size, D3D12_RESOURCE_STATES finalState);

    bool CopyUploadToTexture(ComPtr<ID3D12Resource>& textureResource,
                            ComPtr<ID3D12Resource>& uploadBuffer,
                            uint32 width, uint32 height, DXGI_FORMAT format);

    // Public interface for executing upload commands
    void ExecuteUploadCommands();

    bool CreateVertexBuffer(const void* data, uint64 size, ComPtr<ID3D12Resource>& vertexBuffer,
                           ComPtr<ID3D12Resource>& uploadBuffer, D3D12_VERTEX_BUFFER_VIEW& bufferView);

    bool CreateIndexBuffer(const void* data, uint64 size, ComPtr<ID3D12Resource>& indexBuffer,
                          ComPtr<ID3D12Resource>& uploadBuffer, D3D12_INDEX_BUFFER_VIEW& bufferView);

    bool CreateConstantBuffer(uint64 size, ComPtr<ID3D12Resource>& constantBuffer, void** mappedData = nullptr);

    // Shader compilation
    bool CompileShader(const String& source, const String& entryPoint, const String& target,
                      ComPtr<ID3DBlob>& shaderBlob);
    
    // Shader loading from files  
    bool LoadShaderSource(const String& filePath, String& shaderSource);
    bool CreateBasicMeshShaders();

    // Root Signature creation
    bool CreateRootSignatures();
    
    // PSO creation - updated to use internal shaders
    bool CreateAllPipelineStates();
    
    // Initialize rendering pipeline with shader loading
    bool InitializeRenderingPipeline();
    
    // Root Signature accessors
    ID3D12RootSignature* GetBasicMeshRootSignature() const { return m_basicMeshRootSignature.Get(); }
    ID3D12RootSignature* GetTexturedMeshRootSignature() const { return m_texturedMeshRootSignature.Get(); }
    
    // PSO accessors
    ID3D12PipelineState* GetBasicMeshPSO() const { return m_basicMeshPSO.Get(); }
    ID3D12PipelineState* GetWireframeMeshPSO() const { return m_wireframeMeshPSO.Get(); }
    ID3D12PipelineState* GetTexturedMeshPSO() const { return m_texturedMeshPSO.Get(); }
    ID3D12PipelineState* GetTexturedWireframeMeshPSO() const { return m_texturedWireframeMeshPSO.Get(); }
    ID3D12PipelineState* GetEmissiveMeshPSO() const { return m_emissiveMeshPSO.Get(); }
    ID3D12PipelineState* GetEmissiveWireframeMeshPSO() const { return m_emissiveWireframeMeshPSO.Get(); }
    
    // Shader accessors
    ID3DBlob* GetVertexShader() const { return m_vertexShader.Get(); }
    ID3DBlob* GetPixelShader() const { return m_pixelShader.Get(); }
    ID3DBlob* GetTexturedPixelShader() const { return m_texturedPixelShader.Get(); }
    ID3DBlob* GetEmissivePixelShader() const { return m_emissivePixelShader.Get(); }
    
    // Constant buffer management
    bool CreateAllConstantBuffers();
    
    // Model constant buffer allocation
    uint32 AllocateObjectIndex();
    void ResetObjectIndex() { m_currentObjectIndex = 0; }
    void UpdateModelConstants(const DirectX::XMMATRIX& modelMatrix, uint32 objectIndex);
    void UpdateViewConstants(const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projMatrix, const DirectX::XMFLOAT3& cameraPos);
    void UpdateLightConstants(const DirectX::XMFLOAT3& lightPos, const DirectX::XMFLOAT3& lightColor, float intensity);
    void UpdateMaterialConstants(const DirectX::XMFLOAT3& baseColor, uint32 objectIndex, float metallic = 0.0f, float roughness = 0.5f);
    
    // Constant buffer accessors for binding
    ID3D12Resource* GetModelConstantBuffer(uint32 objectIndex) const;
    ID3D12Resource* GetViewConstantBuffer() const { return m_viewConstantBuffer.Get(); }
    ID3D12Resource* GetLightConstantBuffer() const { return m_lightConstantBuffer.Get(); }
    ID3D12Resource* GetMaterialConstantBuffer(uint32 objectIndex) const;

    // Descriptor heap access
    bool CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 numDescriptors,
                             D3D12_DESCRIPTOR_HEAP_FLAGS flags, ComPtr<ID3D12DescriptorHeap>& heap);

    uint32 GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;
    
    // Shader resource descriptor heaps
    bool CreateShaderDescriptorHeaps();
    ID3D12DescriptorHeap* GetSRVHeap() const { return m_srvHeap.Get(); }
    ID3D12DescriptorHeap* GetSamplerHeap() const { return m_samplerHeap.Get(); }
    
    // Mesh rendering methods (moved from ShaderManager)
    void BindForMeshRendering(ID3D12GraphicsCommandList* commandList, uint32 objectIndex = 0);
    void BindForTexturedMeshRendering(ID3D12GraphicsCommandList* commandList, uint32 objectIndex);
    void BindForEmissiveMeshRendering(ID3D12GraphicsCommandList* commandList, uint32 objectIndex);
    
    // Render mode control
    void SetWireframeMode(bool wireframe) { m_wireframeMode = wireframe; }
    bool IsWireframeMode() const { return m_wireframeMode; }
    
    // Descriptor allocation and handle management
    uint32 AllocateSRVDescriptor();
    uint32 AllocateSamplerDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUHandle(uint32 index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle(uint32 index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCPUHandle(uint32 index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGPUHandle(uint32 index) const;

    // Device access for external classes
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    
    // RHI integration
    class DX12RHIContext* CreateRHIContext();
    void DestroyRHIContext(class DX12RHIContext* context);

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
    // Root Signature creation helpers
    bool CreateBasicMeshRootSignature();
    bool CreateTexturedMeshRootSignature();
    
    // PSO creation helpers
    bool CreateBasicMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader);
    bool CreateWireframeMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader);
    bool CreateTexturedMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader);
    bool CreateTexturedWireframeMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader);
    bool CreateEmissiveMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader);
    bool CreateEmissiveWireframeMeshPSO(ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader);
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

    // Root Signatures
    ComPtr<ID3D12RootSignature> m_basicMeshRootSignature;
    ComPtr<ID3D12RootSignature> m_texturedMeshRootSignature;
    
    // Pipeline State Objects
    ComPtr<ID3D12PipelineState> m_basicMeshPSO;
    ComPtr<ID3D12PipelineState> m_wireframeMeshPSO;
    ComPtr<ID3D12PipelineState> m_texturedMeshPSO;
    ComPtr<ID3D12PipelineState> m_texturedWireframeMeshPSO;
    ComPtr<ID3D12PipelineState> m_emissiveMeshPSO;
    ComPtr<ID3D12PipelineState> m_emissiveWireframeMeshPSO;

    // Constant Buffers
    static const uint32 MAX_OBJECTS = 256;
    Vector<ComPtr<ID3D12Resource>> m_modelConstantBuffers;
    Vector<ModelConstants*> m_mappedModelConstants;
    uint32 m_currentObjectIndex = 0;
    
    ComPtr<ID3D12Resource> m_viewConstantBuffer;
    ComPtr<ID3D12Resource> m_lightConstantBuffer;
    Vector<ComPtr<ID3D12Resource>> m_materialConstantBuffers;
    
    ViewConstants* m_mappedViewConstants = nullptr;
    LightConstants* m_mappedLightConstants = nullptr;
    Vector<MaterialConstants*> m_mappedMaterialConstants;

    // Shader Resource Descriptor Heaps
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
    uint32 m_srvDescriptorSize = 0;
    uint32 m_samplerDescriptorSize = 0;
    uint32 m_currentSrvIndex = 0;
    uint32 m_currentSamplerIndex = 0;
    
    // Shaders (moved from ShaderManager)
    ComPtr<ID3DBlob> m_vertexShader;
    ComPtr<ID3DBlob> m_pixelShader;
    ComPtr<ID3DBlob> m_texturedPixelShader;
    ComPtr<ID3DBlob> m_emissivePixelShader;
    bool m_wireframeMode = false;

    // Debug
    ComPtr<ID3D12Debug> m_debugController;
    ComPtr<ID3D12DebugDevice> m_debugDevice;

    DECLARE_NON_COPYABLE(DX12Renderer);
};