#pragma once

#include "Component.h"
#include <DirectXMath.h>

class TransformComponent : public Component {
public:
    TransformComponent();
    TransformComponent(const DirectX::XMFLOAT3& position, 
                      const DirectX::XMFLOAT3& rotation = {0.0f, 0.0f, 0.0f}, 
                      const DirectX::XMFLOAT3& scale = {1.0f, 1.0f, 1.0f});
    virtual ~TransformComponent() = default;

    // Position
    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    void SetPosition(const DirectX::XMFLOAT3& position);
    void SetPosition(float x, float y, float z);
    void AddPosition(const DirectX::XMFLOAT3& offset);
    void AddPosition(float x, float y, float z);

    // Rotation (in radians)
    const DirectX::XMFLOAT3& GetRotation() const { return m_rotation; }
    void SetRotation(const DirectX::XMFLOAT3& rotation);
    void SetRotation(float x, float y, float z);
    void AddRotation(const DirectX::XMFLOAT3& rotation);
    void AddRotation(float x, float y, float z);

    // Scale
    const DirectX::XMFLOAT3& GetScale() const { return m_scale; }
    void SetScale(const DirectX::XMFLOAT3& scale);
    void SetScale(float x, float y, float z);
    void SetScale(float uniformScale);

    // Matrix calculations
    DirectX::XMMATRIX GetWorldMatrix() const;
    DirectX::XMMATRIX GetTranslationMatrix() const;
    DirectX::XMMATRIX GetRotationMatrix() const;
    DirectX::XMMATRIX GetScaleMatrix() const;

    // Direction vectors
    DirectX::XMFLOAT3 GetForward() const;
    DirectX::XMFLOAT3 GetRight() const;
    DirectX::XMFLOAT3 GetUp() const;

private:
    DirectX::XMFLOAT3 m_position = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_rotation = {0.0f, 0.0f, 0.0f}; // Roll, Pitch, Yaw in radians
    DirectX::XMFLOAT3 m_scale = {1.0f, 1.0f, 1.0f};
    
    mutable bool m_worldMatrixDirty = true;
    mutable DirectX::XMFLOAT4X4 m_cachedWorldMatrix;
};