#pragma once

#include "../Core/Utilities/Types.h"
#include "../Platform/Windows/WindowsPlatform.h"
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
    DirectX::XMFLOAT3 lightDirection;
    float lightIntensity;
    DirectX::XMFLOAT3 lightColor;
    float padding;
};

// Shader types
enum class ShaderType {
    Vertex,
    Pixel,
    Geometry,
    Compute
};

// Shader manager class
class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    bool Initialize(class DX12Renderer* renderer);
    void Shutdown();

    // Shader compilation and loading
    bool CompileShaderFromFile(const String& filePath, const String& entryPoint,
                              const String& target, ComPtr<ID3DBlob>& shaderBlob);
    bool LoadCompiledShader(const String& filePath, ComPtr<ID3DBlob>& shaderBlob);

    // Create basic shaders for mesh rendering
    bool CreateBasicMeshShaders();

    // Pipeline state creation
    bool CreateBasicMeshPSO();

    // Object rendering management
    uint32 BeginObjectRender();

    // Bind resources for rendering
    void BindForMeshRendering(ID3D12GraphicsCommandList* commandList);
    void BindForMeshRendering(ID3D12GraphicsCommandList* commandList, uint32 objectIndex);

    // Update constant buffers
    void UpdateModelConstants(const DirectX::XMMATRIX& modelMatrix);
    void UpdateModelConstants(const DirectX::XMMATRIX& modelMatrix, uint32 objectIndex);
    void UpdateViewConstants(const DirectX::XMMATRIX& viewMatrix,
                           const DirectX::XMMATRIX& projMatrix,
                           const DirectX::XMFLOAT3& cameraPos);
    void UpdateLightConstants(const DirectX::XMFLOAT3& lightDir,
                            const DirectX::XMFLOAT3& lightColor,
                            float intensity);

    // Accessors
    ID3D12PipelineState* GetBasicMeshPSO() const { return m_basicMeshPSO.Get(); }
    ID3D12RootSignature* GetBasicMeshRootSignature() const { return m_basicMeshRootSignature.Get(); }

private:
    // Helper methods
    bool CreateRootSignature();
    bool CreateConstantBuffers();
    void CreateDefaultShaderCode();

private:
    DX12Renderer* m_renderer = nullptr;

    static const uint32 MAX_OBJECTS = 256;

    // Shaders
    ComPtr<ID3DBlob> m_vertexShader;
    ComPtr<ID3DBlob> m_pixelShader;

    // Pipeline state and root signature
    ComPtr<ID3D12RootSignature> m_basicMeshRootSignature;
    ComPtr<ID3D12PipelineState> m_basicMeshPSO;

    // Множественные model constant buffers для поддержки нескольких объектов
    Vector<ComPtr<ID3D12Resource>> m_modelConstantBuffers;
    Vector<ModelConstants*> m_mappedModelConstants;
    uint32 m_currentObjectIndex = 0;

    // Остальные constant buffers (общие для всех объектов)
    ComPtr<ID3D12Resource> m_viewConstantBuffer;
    ComPtr<ID3D12Resource> m_lightConstantBuffer;

    // Mapped constant buffer data
    ViewConstants* m_mappedViewConstants = nullptr;
    LightConstants* m_mappedLightConstants = nullptr;

    // Descriptor heap for CBVs
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap;
    uint32 m_cbvDescriptorSize = 0;

    bool m_initialized = false;

    DECLARE_NON_COPYABLE(ShaderManager);
};