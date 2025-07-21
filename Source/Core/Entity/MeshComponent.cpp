#include "MeshComponent.h"
#include "Entity.h"
#include "TransformComponent.h"
#include "../../Rendering/Dx12/DX12Renderer.h"
#include "../../Rendering/ShaderManager.h"
#include "../Scene/Scene.h"
#include "../../Platform/Windows/WindowsPlatform.h"

MeshComponent::MeshComponent() {
    Platform::OutputDebugMessage("MeshComponent created\n");
}

void MeshComponent::Initialize() {
    Component::Initialize();
    m_cachedTransform = GetTransformComponent();
    Platform::OutputDebugMessage("MeshComponent initialized\n");
}

void MeshComponent::Render(DX12Renderer* renderer) {
    if (!m_isVisible || !m_mesh || !renderer) {
        return;
    }

    TransformComponent* transform = GetTransformComponent();
    if (!transform) {
        Platform::OutputDebugMessage("MeshComponent: No transform component found\n");
        return;
    }

    Entity* owner = GetOwner();
    if (!owner || !owner->GetScene()) {
        Platform::OutputDebugMessage("MeshComponent: No owner or scene\n");
        return;
    }

    ShaderManager* shaderManager = owner->GetScene()->GetShaderManager();
    if (!shaderManager) {
        Platform::OutputDebugMessage("MeshComponent: No shader manager in scene\n");
        return;
    }

    // Загружаем данные меша если нужно
    if (m_mesh->NeedsUpload()) {
        if (!m_mesh->UploadData(renderer)) {
            Platform::OutputDebugMessage("MeshComponent: Failed to upload mesh data\n");
            return;
        }
    }

    ID3D12GraphicsCommandList* commandList = renderer->GetCommandList();
    if (!commandList) {
        Platform::OutputDebugMessage("MeshComponent: Invalid command list\n");
        return;
    }

    // Обновляем model константы для этого объекта
    DirectX::XMMATRIX modelMatrix = transform->GetWorldMatrix();
    shaderManager->UpdateModelConstants(modelMatrix);

    // Устанавливаем pipeline state и root signature (если еще не установлены)
    shaderManager->BindForMeshRendering(commandList);

    // Рендерим меш
    m_mesh->Draw(commandList);
}

void MeshComponent::SetMesh(SharedPtr<Mesh> mesh) {
    m_mesh = mesh;
    Platform::OutputDebugMessage("MeshComponent: Mesh set\n");
}

bool MeshComponent::CreateCube(DX12Renderer* renderer) {
    if (!renderer) {
        Platform::OutputDebugMessage("MeshComponent: Invalid renderer for cube creation\n");
        return false;
    }

    Platform::OutputDebugMessage("MeshComponent: Creating cube mesh...\n");
    
    m_mesh = std::make_shared<Mesh>();
    if (!m_mesh->CreateCube(renderer)) {
        Platform::OutputDebugMessage("MeshComponent: Failed to create cube mesh\n");
        m_mesh.reset();
        return false;
    }
    
    Platform::OutputDebugMessage("MeshComponent: Cube mesh created successfully\n");
    return true;
}

bool MeshComponent::LoadFromFile(DX12Renderer* renderer, const String& filePath) {
    if (!renderer) {
        Platform::OutputDebugMessage("MeshComponent: Invalid renderer for file loading\n");
        return false;
    }

    Platform::OutputDebugMessage("MeshComponent: Loading mesh from file: " + filePath + "\n");
    
    m_mesh = std::make_shared<Mesh>();
    if (!m_mesh->LoadFromFile(filePath, renderer)) {
        Platform::OutputDebugMessage("MeshComponent: Failed to load mesh from file: " + filePath + "\n");
        m_mesh.reset();
        return false;
    }
    
    Platform::OutputDebugMessage("MeshComponent: Mesh loaded successfully from file\n");
    return true;
}

TransformComponent* MeshComponent::GetTransformComponent() const {
    if (m_cachedTransform) {
        return m_cachedTransform;
    }

    Entity* owner = GetOwner();
    if (!owner) {
        return nullptr;
    }

    // Кешируем transform компонент для производительности
    m_cachedTransform = owner->GetComponent<TransformComponent>();
    return m_cachedTransform;
}