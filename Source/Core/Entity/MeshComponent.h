#pragma once

#include "Component.h"
#include "../../Rendering/Mesh.h"
#include <memory>

// Forward declarations
class DX12Renderer;
class TransformComponent;

class MeshComponent : public Component {
public:
    MeshComponent();
    virtual ~MeshComponent() = default;

    // Component lifecycle
    void Initialize() override;
    void Render(DX12Renderer* renderer) override;
    void Render(class IRHIContext& context);

    // Mesh management
    void SetMesh(SharedPtr<Mesh> mesh);
    SharedPtr<Mesh> GetMesh() const { return m_mesh; }
    bool HasMesh() const { return m_mesh != nullptr; }
    
    // Material management
    void SetMaterial(SharedPtr<class Material> material);
    SharedPtr<class Material> GetMaterial() const { return m_material; }
    bool HasMaterial() const { return m_material != nullptr; }
    
    // Texture shortcuts
    void SetTexture(const String& texturePath, DX12Renderer* renderer);

    // Factory methods for common meshes
    bool CreateCube(DX12Renderer* renderer);
    bool CreateSphere(DX12Renderer* renderer, uint32 stacks = 20, uint32 slices = 20);
    bool LoadFromFile(DX12Renderer* renderer, const String& filePath);

    // Rendering properties
    bool IsVisible() const { return m_isVisible; }
    void SetVisible(bool visible) { m_isVisible = visible; }

    bool CastsShadows() const { return m_castsShadows; }
    void SetCastsShadows(bool castsShadows) { m_castsShadows = castsShadows; }

    // Material properties (for future expansion)
    const DirectX::XMFLOAT3& GetColor() const { return m_color; }
    void SetColor(const DirectX::XMFLOAT3& color) { m_color = color; }
    void SetColor(float r, float g, float b) { m_color = {r, g, b}; }

private:
    SharedPtr<Mesh> m_mesh;
    SharedPtr<class Material> m_material;
    bool m_isVisible = true;
    bool m_castsShadows = true;
    DirectX::XMFLOAT3 m_color = {1.0f, 1.0f, 1.0f}; // White by default

    // Cached transform component for performance
    mutable TransformComponent* m_cachedTransform = nullptr;
    TransformComponent* GetTransformComponent() const;
};