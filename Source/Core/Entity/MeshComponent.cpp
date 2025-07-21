#include "MeshComponent.h"
#include "Entity.h"
#include "TransformComponent.h"
#include "../../Rendering/Dx12/DX12Renderer.h"
#include "../../Rendering/ShaderManager.h"
#include "../Scene/Scene.h"
#include "../../Platform/Windows/WindowsPlatform.h"

MeshComponent::MeshComponent() {
}

void MeshComponent::Initialize() {
    Component::Initialize();
    m_cachedTransform = GetTransformComponent();
}

void MeshComponent::Render(DX12Renderer* renderer) {
    if (!m_isVisible || !m_mesh || !renderer) return;

    TransformComponent* transform = GetTransformComponent();
    if (!transform) return;

    Entity* owner = GetOwner();
    if (!owner || !owner->GetScene()) return;

    ShaderManager* shaderManager = owner->GetScene()->GetShaderManager();
    if (!shaderManager) return;

    ID3D12GraphicsCommandList* commandList = renderer->GetCommandList();

    DirectX::XMMATRIX modelMatrix = transform->GetWorldMatrix();
    shaderManager->UpdateModelConstants(modelMatrix);

    shaderManager->BindForMeshRendering(commandList);
    m_mesh->Draw(commandList);
}

void MeshComponent::SetMesh(SharedPtr<Mesh> mesh) {
    m_mesh = mesh;
}

bool MeshComponent::CreateCube(DX12Renderer* renderer) {
    if (!renderer) return false;

    m_mesh = std::make_shared<Mesh>();
    return m_mesh->CreateCube(renderer);
}

bool MeshComponent::LoadFromFile(DX12Renderer* renderer, const String& filePath) {
    if (!renderer) return false;

    m_mesh = std::make_shared<Mesh>();
    return m_mesh->LoadFromFile(filePath, renderer);
}

TransformComponent* MeshComponent::GetTransformComponent() const {
    if (m_cachedTransform) return m_cachedTransform;

    Entity* owner = GetOwner();
    if (!owner) return nullptr;

    return owner->GetComponent<TransformComponent>();
}