#include "MeshComponent.h"
#include "Entity.h"
#include "TransformComponent.h"
#include "../../Rendering/Dx12/DX12Renderer.h"
#include "../../Rendering/RHI/IRHIContext.h"
#include "../../Rendering/RHI/DX12RHIContext.h"
#include "../../Rendering/Material.h"
#include "../../Rendering/Bindable/Texture.h"
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
    if (!m_isVisible || !m_mesh || !renderer) return;

    TransformComponent* transform = GetTransformComponent();
    if (!transform) return;

    Entity* owner = GetOwner();
    if (!owner || !owner->GetScene()) return;

    // Upload mesh data if needed
    if (m_mesh->NeedsUpload()) {
        m_mesh->UploadData(renderer);
    }

    ID3D12GraphicsCommandList* commandList = renderer->GetCommandList();

    uint32 objectIndex = renderer->AllocateObjectIndex();

    DirectX::XMMATRIX modelMatrix = transform->GetWorldMatrix();
    renderer->UpdateModelConstants(modelMatrix, objectIndex);
    
    Platform::OutputDebugMessage("MeshComponent: " + owner->GetName() + " assigned objectIndex=" + std::to_string(objectIndex) + "\n");

    // Choose appropriate rendering pipeline based on material
    bool hasMaterial = m_material && m_material->IsValid();
    bool hasTextures = hasMaterial && m_material->GetTexture("DiffuseTexture") != nullptr;
    bool isEmissive = hasMaterial && (m_material->GetName().find("Light") != String::npos || 
                                     m_material->GetName().find("Emissive") != String::npos);
    
    if (isEmissive) {
        Platform::OutputDebugMessage("MeshComponent: Rendering with emissive material: " + m_material->GetName() + "\n");
        
        // Extract color from material and update material constants
        DirectX::XMFLOAT4 materialColor;
        if (m_material->GetParameter("Color", materialColor)) {
            DirectX::XMFLOAT3 baseColor = {materialColor.x, materialColor.y, materialColor.z};
            renderer->UpdateMaterialConstants(baseColor, objectIndex);
            Platform::OutputDebugMessage("MeshComponent: " + owner->GetName() + " objectIndex=" + std::to_string(objectIndex) + 
                                        " updated emissive color: (" + 
                                        std::to_string(baseColor.x) + ", " + 
                                        std::to_string(baseColor.y) + ", " + 
                                        std::to_string(baseColor.z) + ")\n");
        } else {
            // Default bright white for emissive if no color parameter
            renderer->UpdateMaterialConstants({1.0f, 1.0f, 1.0f}, objectIndex);
            Platform::OutputDebugMessage("MeshComponent: " + owner->GetName() + " objectIndex=" + std::to_string(objectIndex) + 
                                        " using default emissive white\n");
        }
        renderer->BindForEmissiveMeshRendering(commandList, objectIndex);
    } else if (hasTextures) {
        Platform::OutputDebugMessage("MeshComponent: Rendering with textured material: " + m_material->GetName() + "\n");
        
        // Use textured pipeline
        renderer->BindForTexturedMeshRendering(commandList, objectIndex);
        
        // Bind material (textures and samplers) - SRV descriptor heap is now implemented
        Platform::OutputDebugMessage("MeshComponent: Binding material with SRV descriptor heap\n");
        DX12RHIContext rhiContext(renderer);
        m_material->Bind(rhiContext);
    } else {
        // Use basic non-textured pipeline
        if (hasMaterial) {
            Platform::OutputDebugMessage("MeshComponent: Rendering with non-textured material: " + m_material->GetName() + "\n");
            
            // Extract color from material and update material constants
            DirectX::XMFLOAT4 materialColor;
            if (m_material->GetParameter("Color", materialColor)) {
                DirectX::XMFLOAT3 baseColor = {materialColor.x, materialColor.y, materialColor.z};
                renderer->UpdateMaterialConstants(baseColor, objectIndex);
                Platform::OutputDebugMessage("MeshComponent: " + owner->GetName() + " objectIndex=" + std::to_string(objectIndex) + 
                                            " updated material color: (" + 
                                            std::to_string(baseColor.x) + ", " + 
                                            std::to_string(baseColor.y) + ", " + 
                                            std::to_string(baseColor.z) + ")\n");
            } else {
                // Default white color if no color parameter found
                renderer->UpdateMaterialConstants({1.0f, 1.0f, 1.0f}, objectIndex);
                Platform::OutputDebugMessage("MeshComponent: " + owner->GetName() + " objectIndex=" + std::to_string(objectIndex) + 
                                            " using default white color\n");
            }
        } else {
            Platform::OutputDebugMessage("MeshComponent: " + owner->GetName() + " objectIndex=" + std::to_string(objectIndex) + 
                                        " rendering without material\n");
            // Default gray color for objects without material
            renderer->UpdateMaterialConstants({0.7f, 0.7f, 0.7f}, objectIndex);
        }
        renderer->BindForMeshRendering(commandList, objectIndex);
    }

    m_mesh->Draw(commandList);
}

void MeshComponent::Render(IRHIContext& context) {
    if (!m_isVisible || !m_mesh) return;

    TransformComponent* transform = GetTransformComponent();
    if (!transform) return;

    // Bind material first (shaders and textures)
    if (m_material && m_material->IsValid()) {
        m_material->Bind(context);
    }

    // Bind mesh geometry
    m_mesh->Bind(context);

    // TODO: Update constant buffers for transform
    // This will need to be implemented when ShaderManager is updated
    
    // Draw using RHI context
    context.DrawIndexed(m_mesh->GetIndexCount());
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

bool MeshComponent::CreateSphere(DX12Renderer* renderer, uint32 stacks, uint32 slices) {
    if (!renderer) {
        Platform::OutputDebugMessage("MeshComponent: Invalid renderer for sphere creation\n");
        return false;
    }

    Platform::OutputDebugMessage("MeshComponent: Creating sphere mesh...\n");

    m_mesh = std::make_shared<Mesh>();
    if (!m_mesh->CreateSphere(renderer, stacks, slices)) {
        Platform::OutputDebugMessage("MeshComponent: Failed to create sphere mesh\n");
        m_mesh.reset();
        return false;
    }

    Platform::OutputDebugMessage("MeshComponent: Sphere mesh created successfully\n");
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

void MeshComponent::SetMaterial(SharedPtr<Material> material) {
    m_material = material;
    Platform::OutputDebugMessage("MeshComponent: Material set\n");
}

void MeshComponent::SetTexture(const String& texturePath, DX12Renderer* renderer) {
    if (!renderer) {
        Platform::OutputDebugMessage("MeshComponent: Invalid renderer for texture loading\n");
        return;
    }
    
    Platform::OutputDebugMessage("MeshComponent: Loading texture from: " + texturePath + "\n");
    
    try {
        // Create texture using the Texture factory method
        auto texture = Texture::CreateFromFile(*renderer, texturePath, false, "MeshTexture");
        
        if (!texture || !texture->IsValid()) {
            Platform::OutputDebugMessage("MeshComponent: Failed to load texture from: " + texturePath + "\n");
            return;
        }
        
        Platform::OutputDebugMessage("MeshComponent: Texture loaded successfully\n");
        
        // If no material exists, create a default textured material
        if (!m_material) {
            Platform::OutputDebugMessage("MeshComponent: Creating new textured material\n");
            m_material = Material::CreateTextured(*renderer, std::move(texture), "AutoGeneratedMaterial");
            Platform::OutputDebugMessage("MeshComponent: Created new textured material\n");
        } else {
            // Set texture on existing material
            Platform::OutputDebugMessage("MeshComponent: Applying texture to existing material\n");
            m_material->SetTexture("DiffuseTexture", std::move(texture));
            Platform::OutputDebugMessage("MeshComponent: Applied texture to existing material\n");
        }
    } catch (const std::exception& e) {
        Platform::OutputDebugMessage("MeshComponent: Exception in SetTexture: " + String(e.what()) + "\n");
    } catch (...) {
        Platform::OutputDebugMessage("MeshComponent: Unknown exception in SetTexture\n");
    }
}

TransformComponent* MeshComponent::GetTransformComponent() const {
    if (m_cachedTransform) {
        return m_cachedTransform;
    }

    Entity* owner = GetOwner();
    if (!owner) {
        return nullptr;
    }

    m_cachedTransform = owner->GetComponent<TransformComponent>();
    return m_cachedTransform;
}