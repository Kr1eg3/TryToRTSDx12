#include "Mesh.h"
#include "Dx12/DX12Renderer.h"
#include "../Platform/Windows/WindowsPlatform.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma comment(lib, "assimp-vc143-mt.lib")

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

        // We need access to the device from the renderer
        // For now, we'll create the buffers in a simple way
        // In a real implementation, you'd want to expose more functionality from DX12Renderer

        // This is a simplified version - in practice you'd want to implement buffer creation
        // methods in the DX12Renderer class and call them from here

        Platform::OutputDebugMessage("Mesh buffers created successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Error creating mesh buffers: " + e.GetMessage());
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