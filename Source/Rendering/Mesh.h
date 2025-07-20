#pragma once

#include "../Core/Utilities/Types.h"
#include "../Platform/Windows/WindowsPlatform.h"
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
    //bool CreateSphere(class DX12Renderer* renderer, uint32 stacks = 20, uint32 slices = 20);
    //bool CreatePlane(class DX12Renderer* renderer);

    // Rendering
    void Draw(ID3D12GraphicsCommandList* commandList);

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

    // D3D12 resources
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    ComPtr<ID3D12Resource> m_vertexBufferUpload;
    ComPtr<ID3D12Resource> m_indexBufferUpload;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};

    // Helper methods
    bool CreateBuffers(class DX12Renderer* renderer);
    void CreateCubeVertices();

    DECLARE_NON_COPYABLE(Mesh);
};