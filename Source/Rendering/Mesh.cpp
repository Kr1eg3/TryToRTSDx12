#include "Mesh.h"
#include "Dx12/DX12Renderer.h"
#include "RHI/DX12RHIContext.h"
#include "../Platform/Windows/WindowsPlatform.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//#pragma comment(lib, "assimp-vc143-mt.lib")

Mesh::Mesh() {
    Platform::OutputDebugMessage("Mesh created\n");
}

Mesh::~Mesh() {
    // Resources are automatically released by ComPtr
}

bool Mesh::LoadFromFile(const String& filePath, DX12Renderer* renderer) {
    Platform::OutputDebugMessage("Loading mesh from file: " + filePath + "\n");

    Assimp::Importer importer;

    // Configure importer
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
        aiPrimitiveType_POINT | aiPrimitiveType_LINE);

    // Load scene
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes |
        aiProcess_ValidateDataStructure
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Platform::OutputDebugMessage("Error loading mesh: " + String(importer.GetErrorString()) + "\n");
        return false;
    }

    if (scene->mNumMeshes == 0) {
        Platform::OutputDebugMessage("No meshes found in file\n");
        return false;
    }

    // For simplicity, load only the first mesh
    const aiMesh* mesh = scene->mMeshes[0];

    Platform::OutputDebugMessage("Mesh loaded - Vertices: " + std::to_string(mesh->mNumVertices) +
                                 ", Faces: " + std::to_string(mesh->mNumFaces) + "\n");

    // Clear existing data
    m_vertices.clear();
    m_indices.clear();

    // Load vertices
    m_vertices.reserve(mesh->mNumVertices);
    for (uint32 i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;

        // Position
        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

        // Normal
        if (mesh->HasNormals()) {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        } else {
            vertex.normal = { 0.0f, 1.0f, 0.0f };
        }

        // Texture coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoord.x = mesh->mTextureCoords[0][i].x;
            vertex.texCoord.y = mesh->mTextureCoords[0][i].y;
        } else {
            vertex.texCoord = { 0.0f, 0.0f };
        }

        m_vertices.push_back(vertex);
    }

    // Load indices
    m_indices.reserve(mesh->mNumFaces * 3);
    for (uint32 i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices == 3) {
            m_indices.push_back(face.mIndices[0]);
            m_indices.push_back(face.mIndices[1]);
            m_indices.push_back(face.mIndices[2]);
        }
    }

    m_vertexCount = static_cast<uint32>(m_vertices.size());
    m_indexCount = static_cast<uint32>(m_indices.size());

    // Create D3D12 buffers
    return CreateBuffers(renderer);
}

bool Mesh::CreateCube(DX12Renderer* renderer) {
    Platform::OutputDebugMessage("Creating cube mesh\n");

    // Clear existing data
    m_vertices.clear();
    m_indices.clear();

    CreateCubeVertices();

    m_vertexCount = static_cast<uint32>(m_vertices.size());
    m_indexCount = static_cast<uint32>(m_indices.size());

    // Create D3D12 buffers
    return CreateBuffers(renderer);
}

void Mesh::CreateCubeVertices() {
    // Cube vertices with normals and UVs
    m_vertices = {
        // Front face
        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },

        // Back face
        { {  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },

        // Top face
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },

        // Bottom face
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },

        // Right face
        { {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },

        // Left face
        { { -1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -1.0f, -1.0f,  1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -1.0f,  1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } }
    };

    // Cube indices
    m_indices = {
        // Front face
        0, 1, 2,  2, 3, 0,
        // Back face
        4, 5, 6,  6, 7, 4,
        // Top face
        8, 9, 10,  10, 11, 8,
        // Bottom face
        12, 13, 14,  14, 15, 12,
        // Right face
        16, 17, 18,  18, 19, 16,
        // Left face
        20, 21, 22,  22, 23, 20
    };
}

bool Mesh::CreateBuffers(DX12Renderer* renderer) {
    if (!renderer || m_vertices.empty() || m_indices.empty()) {
        Platform::OutputDebugMessage("Error: Invalid renderer or empty mesh data\n");
        return false;
    }

    try {
        Platform::OutputDebugMessage("Creating mesh buffers...\n");

        // Create bindable objects
        m_vertexBuffer = std::make_unique<VertexBuffer<Vertex>>(*renderer, m_vertices, "MeshVertexBuffer");
        m_indexBuffer = std::make_unique<IndexBuffer>(*renderer, m_indices, "MeshIndexBuffer");
        
        if (!m_vertexBuffer->IsValid() || !m_indexBuffer->IsValid()) {
            Platform::OutputDebugMessage("Failed to create bindable buffers\n");
            return false;
        }

        // Legacy fallback - create old-style buffers for compatibility
        uint64 vertexBufferSize = m_vertices.size() * sizeof(Vertex);
        uint64 indexBufferSize = m_indices.size() * sizeof(uint32);

        if (!renderer->CreateBuffer(vertexBufferSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                                   m_legacyVertexBuffer, m_vertices.data(), &m_vertexBufferUpload)) {
            Platform::OutputDebugMessage("Failed to create legacy vertex buffer\n");
            return false;
        }

        // Setup vertex buffer view
        m_vertexBufferView.BufferLocation = m_legacyVertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);

        // Create index buffer (this will create both default and upload buffers)
        if (!renderer->CreateBuffer(indexBufferSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
                                   m_legacyIndexBuffer, m_indices.data(), &m_indexBufferUpload)) {
            Platform::OutputDebugMessage("Failed to create index buffer\n");
            return false;
        }

        // Setup index buffer view
        m_indexBufferView.BufferLocation = m_legacyIndexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_indexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);

        // Mark that we need to copy data (this will be done when command list is recording)
        m_needsUpload = true;
        m_vertexBufferSize = vertexBufferSize;
        m_indexBufferSize = indexBufferSize;

        Platform::OutputDebugMessage("Mesh buffers created successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating mesh buffers: " + e.GetMessage());
        return false;
    }
}

bool Mesh::UploadData(DX12Renderer* renderer) {
    if (!m_needsUpload || !renderer) {
        return true; // Nothing to upload or invalid renderer
    }

    try {
        // Copy vertex buffer
        if (m_legacyVertexBuffer && m_vertexBufferUpload) {
            if (!renderer->CopyUploadToDefaultBuffer(m_legacyVertexBuffer, m_vertexBufferUpload,
                                                    m_vertexBufferSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)) {
                return false;
            }
        }

        // Copy index buffer
        if (m_legacyIndexBuffer && m_indexBufferUpload) {
            if (!renderer->CopyUploadToDefaultBuffer(m_legacyIndexBuffer, m_indexBufferUpload,
                                                    m_indexBufferSize, D3D12_RESOURCE_STATE_INDEX_BUFFER)) {
                return false;
            }
        }

        m_needsUpload = false;
        Platform::OutputDebugMessage("Mesh data uploaded successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error uploading mesh data: " + e.GetMessage());
        return false;
    }
}

void Mesh::Draw(ID3D12GraphicsCommandList* commandList) {
    if (!commandList || m_indexCount == 0) {
        return;
    }

    // Set vertex and index buffers
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw indexed
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}

void Mesh::Bind(IRHIContext& context) {
    if (!m_vertexBuffer || !m_indexBuffer || m_indexCount == 0) {
        Platform::OutputDebugMessage("Warning: Attempting to bind invalid mesh\n");
        return;
    }

    // Bind using new bindable system
    m_vertexBuffer->Bind(context);
    m_indexBuffer->Bind(context);
    context.SetPrimitiveTopology(RHIPrimitiveTopology::TriangleList);
}

bool Mesh::CreateSphere(DX12Renderer* renderer, uint32 stacks, uint32 slices) {
    if (!renderer) {
        Platform::OutputDebugMessage("Error: Renderer is null\n");
        return false;
    }

    Platform::OutputDebugMessage("Creating sphere mesh with " + std::to_string(stacks) + 
                                " stacks and " + std::to_string(slices) + " slices\n");

    CreateSphereVertices(stacks, slices);

    // Set vertex and index counts
    m_vertexCount = static_cast<uint32>(m_vertices.size());
    m_indexCount = static_cast<uint32>(m_indices.size());

    Platform::OutputDebugMessage("Sphere has " + std::to_string(m_vertexCount) + 
                                " vertices and " + std::to_string(m_indexCount) + " indices\n");

    // Create D3D12 buffers
    if (!CreateBuffers(renderer)) {
        Platform::OutputDebugMessage("Failed to create sphere buffers\n");
        return false;
    }

    m_needsUpload = true;
    Platform::OutputDebugMessage("Sphere mesh created successfully\n");
    return true;
}

void Mesh::CreateSphereVertices(uint32 stacks, uint32 slices) {
    m_vertices.clear();
    m_indices.clear();

    const float PI = 3.14159265f;
    const float radius = 1.0f;

    // Generate vertices
    for (uint32 stack = 0; stack <= stacks; ++stack) {
        float phi = PI * stack / stacks; // From 0 to PI
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        for (uint32 slice = 0; slice <= slices; ++slice) {
            float theta = 2.0f * PI * slice / slices; // From 0 to 2*PI
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            // Spherical coordinates to Cartesian
            DirectX::XMFLOAT3 position;
            position.x = radius * sinPhi * cosTheta;
            position.y = radius * cosPhi;
            position.z = radius * sinPhi * sinTheta;

            // Normal is the same as position for unit sphere
            DirectX::XMFLOAT3 normal = position;

            // Texture coordinates
            DirectX::XMFLOAT2 texCoord;
            texCoord.x = static_cast<float>(slice) / slices;
            texCoord.y = static_cast<float>(stack) / stacks;

            m_vertices.emplace_back(position, normal, texCoord);
        }
    }

    // Generate indices
    for (uint32 stack = 0; stack < stacks; ++stack) {
        for (uint32 slice = 0; slice < slices; ++slice) {
            uint32 first = stack * (slices + 1) + slice;
            uint32 second = first + slices + 1;

            // First triangle
            m_indices.push_back(first);
            m_indices.push_back(second);
            m_indices.push_back(first + 1);

            // Second triangle
            m_indices.push_back(second);
            m_indices.push_back(second + 1);
            m_indices.push_back(first + 1);
        }
    }
}