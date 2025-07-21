#include "TransformComponent.h"
#include <cmath>

using namespace DirectX;

TransformComponent::TransformComponent() {
    // Initialize with identity
}

TransformComponent::TransformComponent(const XMFLOAT3& position, const XMFLOAT3& rotation, const XMFLOAT3& scale)
    : m_position(position), m_rotation(rotation), m_scale(scale) {
}

void TransformComponent::SetPosition(const XMFLOAT3& position) {
    m_position = position;
    m_worldMatrixDirty = true;
}

void TransformComponent::SetPosition(float x, float y, float z) {
    SetPosition({x, y, z});
}

void TransformComponent::AddPosition(const XMFLOAT3& offset) {
    m_position.x += offset.x;
    m_position.y += offset.y;
    m_position.z += offset.z;
    m_worldMatrixDirty = true;
}

void TransformComponent::AddPosition(float x, float y, float z) {
    AddPosition({x, y, z});
}

void TransformComponent::SetRotation(const XMFLOAT3& rotation) {
    m_rotation = rotation;
    m_worldMatrixDirty = true;
}

void TransformComponent::SetRotation(float x, float y, float z) {
    SetRotation({x, y, z});
}

void TransformComponent::AddRotation(const XMFLOAT3& rotation) {
    m_rotation.x += rotation.x;
    m_rotation.y += rotation.y;
    m_rotation.z += rotation.z;
    m_worldMatrixDirty = true;
}

void TransformComponent::AddRotation(float x, float y, float z) {
    AddRotation({x, y, z});
}

void TransformComponent::SetScale(const XMFLOAT3& scale) {
    m_scale = scale;
    m_worldMatrixDirty = true;
}

void TransformComponent::SetScale(float x, float y, float z) {
    SetScale({x, y, z});
}

void TransformComponent::SetScale(float uniformScale) {
    SetScale({uniformScale, uniformScale, uniformScale});
}

XMMATRIX TransformComponent::GetWorldMatrix() const {
    if (m_worldMatrixDirty) {
        XMMATRIX world = GetScaleMatrix() * GetRotationMatrix() * GetTranslationMatrix();
        XMStoreFloat4x4(&m_cachedWorldMatrix, world);
        m_worldMatrixDirty = false;
    }

    return XMLoadFloat4x4(&m_cachedWorldMatrix);
}

XMMATRIX TransformComponent::GetTranslationMatrix() const {
    return XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
}

XMMATRIX TransformComponent::GetRotationMatrix() const {
    return XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
}

XMMATRIX TransformComponent::GetScaleMatrix() const {
    return XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
}

XMFLOAT3 TransformComponent::GetForward() const {
    XMMATRIX rotMatrix = GetRotationMatrix();
    XMVECTOR forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    forward = XMVector3TransformNormal(forward, rotMatrix);

    XMFLOAT3 result;
    XMStoreFloat3(&result, forward);
    return result;
}

XMFLOAT3 TransformComponent::GetRight() const {
    XMMATRIX rotMatrix = GetRotationMatrix();
    XMVECTOR right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    right = XMVector3TransformNormal(right, rotMatrix);

    XMFLOAT3 result;
    XMStoreFloat3(&result, right);
    return result;
}

XMFLOAT3 TransformComponent::GetUp() const {
    XMMATRIX rotMatrix = GetRotationMatrix();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    up = XMVector3TransformNormal(up, rotMatrix);

    XMFLOAT3 result;
    XMStoreFloat3(&result, up);
    return result;
}