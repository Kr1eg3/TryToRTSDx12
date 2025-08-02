#pragma once

#include "../Core/Utilities/Types.h"
#include "../Platform/Windows/WindowsPlatform.h"
#include "Bindable/BindableBase.h"
#include <DirectXMath.h>

// Vertex structure
struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texCoord;

    Vertex() = default;
    Vertex(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& norm, const DirectX::XMFLOAT2& uv)
        : position(pos), normal(norm), texCoord(uv) {}
};

// Mesh class
class Mesh {
public:
    Mesh();
    ~Mesh();

    // Load mesh from file using Assimp
    bool LoadFromFile(const String& filePath, class DX12Renderer* renderer);

    // Create primitive meshes
    bool CreateCube(class DX12Renderer* renderer);
    bool CreateSphere(class DX12Renderer* renderer, uint32 stacks = 20, uint32 slices = 20);
    //bool CreatePlane(class DX12Renderer* renderer);

    // Rendering
    void Draw(ID3D12GraphicsCommandList* commandList);
    void Bind(class IRHIContext& context);

    // Upload data when command list is recording
    bool UploadData(class DX12Renderer* renderer);

    // Check if upload is needed
    bool NeedsUpload() const { return m_needsUpload; }

    // Accessors
    uint32 GetVertexCount() const { return m_vertexCount; }
    uint32 GetIndexCount() const { return m_indexCount; }
    const Vector<Vertex>& GetVertices() const { return m_vertices; }
    const Vector<uint32>& GetIndices() const { return m_indices; }

private:
    // Mesh data
    Vector<Vertex> m_vertices;
    Vector<uint32> m_indices;
    uint32 m_vertexCount = 0;
    uint32 m_indexCount = 0;

    // Bindable objects
    UniquePtr<VertexBuffer<Vertex>> m_vertexBuffer;
    UniquePtr<IndexBuffer> m_indexBuffer;
    
    // Legacy D3D12 resources (for compatibility)
    ComPtr<ID3D12Resource> m_legacyVertexBuffer;
    ComPtr<ID3D12Resource> m_legacyIndexBuffer;
    ComPtr<ID3D12Resource> m_vertexBufferUpload;
    ComPtr<ID3D12Resource> m_indexBufferUpload;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};

    // Upload tracking
    bool m_needsUpload = false;
    uint64 m_vertexBufferSize = 0;
    uint64 m_indexBufferSize = 0;

    // Helper methods
    bool CreateBuffers(class DX12Renderer* renderer);
    void CreateCubeVertices();
    void CreateSphereVertices(uint32 stacks, uint32 slices);

    DECLARE_NON_COPYABLE(Mesh);
};