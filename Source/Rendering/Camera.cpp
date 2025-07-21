#include "Camera.h"
#include "../Core/Window/Window.h"
#include "../Platform/Windows/WindowsPlatform.h"

using namespace DirectX;

Camera::Camera(const CameraDesc& desc)
    : m_position(desc.position)
    , m_worldUp(desc.up)
    , m_fovY(desc.fovY)
    , m_aspectRatio(desc.aspectRatio)
    , m_nearPlane(desc.nearPlane)
    , m_farPlane(desc.farPlane)
    , m_moveSpeed(desc.moveSpeed)
    , m_rotationSpeed(desc.rotationSpeed)
    , m_mouseSensitivity(desc.mouseSensitivity)
    , m_scrollSensitivity(desc.scrollSensitivity)
{
    // Calculate initial orientation from position and target
    XMVECTOR posVec = XMLoadFloat3(&m_position);
    XMVECTOR targetVec = XMLoadFloat3(&desc.target);
    XMVECTOR direction = XMVector3Normalize(XMVectorSubtract(targetVec, posVec));

    // Convert direction to yaw and pitch
    XMFLOAT3 dir;
    XMStoreFloat3(&dir, direction);

    m_yaw = atan2f(dir.z, dir.x);
    m_pitch = asinf(dir.y);

    // Update vectors and matrices
    UpdateVectors();
    UpdateMatrices();

    Platform::OutputDebugMessage("Camera created at position: " +
        std::to_string(m_position.x) + ", " +
        std::to_string(m_position.y) + ", " +
        std::to_string(m_position.z) + "\n");
}

void Camera::Update(float32 deltaTime) {
    bool moved = false;

    // Keyboard movement
    float velocity = m_moveSpeed * deltaTime;

    if (m_keys[static_cast<size_t>(KeyCode::W)]) {
        MoveForward(velocity);
        moved = true;
    }
    if (m_keys[static_cast<size_t>(KeyCode::S)]) {
        MoveForward(-velocity);
        moved = true;
    }
    if (m_keys[static_cast<size_t>(KeyCode::D)]) {
        MoveRight(velocity);
        moved = true;
    }
    if (m_keys[static_cast<size_t>(KeyCode::A)]) {
        MoveRight(-velocity);
        moved = true;
    }
    if (m_keys[static_cast<size_t>(KeyCode::Space)]) {
        MoveUp(velocity);
        moved = true;
    }
    if (m_keys[static_cast<size_t>(KeyCode::Ctrl)]) {
        MoveUp(-velocity);
        moved = true;
    }

    // Speed adjustment
    if (m_keys[static_cast<size_t>(KeyCode::Shift)]) {
        m_moveSpeed = 20.0f; // Fast mode
    } else {
        m_moveSpeed = 10.0f; // Normal speed
    }

    if (moved) {
        m_viewMatrixDirty = true;
    }
}

void Camera::OnKeyEvent(const KeyEvent& event) {
    if (event.key != KeyCode::Unknown) {
        m_keys[static_cast<size_t>(event.key)] = event.pressed;
    }

    // Handle special keys
    if (event.pressed) {
        switch (event.key) {
            case KeyCode::R:
                // Reset camera to default position
                SetPosition({ 0.0f, 0.0f, -5.0f });
                SetTarget({ 0.0f, 0.0f, 0.0f });
                Platform::OutputDebugMessage("Camera reset\n");
                break;

            case KeyCode::F1:
                Platform::OutputDebugMessage("Camera position: " +
                    std::to_string(m_position.x) + ", " +
                    std::to_string(m_position.y) + ", " +
                    std::to_string(m_position.z) + "\n");
                break;
        }
    }
}

void Camera::OnMouseButtonEvent(const MouseButtonEvent& event) {
    if (event.button < MouseButton::Middle) {
        m_mouseButtons[static_cast<size_t>(event.button)] = event.pressed;
    }

    // Reset mouse tracking on button press
    if (event.pressed && event.button == MouseButton::Right) {
        m_firstMouse = true;
    }
}

void Camera::OnMouseMoveEvent(const MouseMoveEvent& event) {
    // Only rotate camera when right mouse button is held
    if (!m_mouseButtons[static_cast<size_t>(MouseButton::Right)]) {
        m_firstMouse = true;
        return;
    }

    // Initialize mouse position on first movement
    if (m_firstMouse) {
        m_lastMouseX = static_cast<float>(event.x);
        m_lastMouseY = static_cast<float>(event.y);
        m_firstMouse = false;
        return;
    }

    // Calculate mouse offset
    float xOffset = static_cast<float>(event.x) - m_lastMouseX;
    float yOffset = m_lastMouseY - static_cast<float>(event.y); // Reversed since y-coordinates go from bottom to top

    m_lastMouseX = static_cast<float>(event.x);
    m_lastMouseY = static_cast<float>(event.y);

    // Apply sensitivity
    xOffset *= m_mouseSensitivity;
    yOffset *= m_mouseSensitivity;

    // Update angles
    Rotate(xOffset, yOffset);
}

void Camera::OnMouseWheelEvent(const MouseWheelEvent& event) {
    // Zoom in/out by moving forward/backward
    float distance = event.delta * m_scrollSensitivity;
    MoveForward(distance);

    Platform::OutputDebugMessage("Camera zoom: " + std::to_string(distance) + "\n");
}

XMMATRIX Camera::GetViewMatrix() const {
    if (m_viewMatrixDirty) {
        UpdateMatrices();
    }
    return m_viewMatrix;
}

XMMATRIX Camera::GetProjectionMatrix() const {
    if (m_projectionMatrixDirty) {
        UpdateMatrices();
    }
    return m_projectionMatrix;
}

XMMATRIX Camera::GetViewProjectionMatrix() const {
    return GetViewMatrix() * GetProjectionMatrix();
}

void Camera::SetPosition(const XMFLOAT3& position) {
    m_position = position;
    m_viewMatrixDirty = true;
}

void Camera::SetTarget(const XMFLOAT3& target) {
    XMVECTOR posVec = XMLoadFloat3(&m_position);
    XMVECTOR targetVec = XMLoadFloat3(&target);
    XMVECTOR direction = XMVector3Normalize(XMVectorSubtract(targetVec, posVec));

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, direction);

    m_yaw = atan2f(dir.z, dir.x);
    m_pitch = asinf(dir.y);

    ClampPitch();
    UpdateVectors();
}

void Camera::SetAspectRatio(float aspectRatio) {
    m_aspectRatio = aspectRatio;
    m_projectionMatrixDirty = true;
}

void Camera::SetFOV(float fovY) {
    m_fovY = fovY;
    m_projectionMatrixDirty = true;
}

void Camera::MoveForward(float distance) {
    XMVECTOR forward = XMLoadFloat3(&m_forward);
    XMVECTOR pos = XMLoadFloat3(&m_position);
    pos = XMVectorAdd(pos, XMVectorScale(forward, distance));
    XMStoreFloat3(&m_position, pos);
    m_viewMatrixDirty = true;
}

void Camera::MoveRight(float distance) {
    XMVECTOR right = XMLoadFloat3(&m_right);
    XMVECTOR pos = XMLoadFloat3(&m_position);
    pos = XMVectorAdd(pos, XMVectorScale(right, distance));
    XMStoreFloat3(&m_position, pos);
    m_viewMatrixDirty = true;
}

void Camera::MoveUp(float distance) {
    XMVECTOR up = XMLoadFloat3(&m_up);
    XMVECTOR pos = XMLoadFloat3(&m_position);
    pos = XMVectorAdd(pos, XMVectorScale(up, distance));
    XMStoreFloat3(&m_position, pos);
    m_viewMatrixDirty = true;
}

void Camera::Rotate(float yaw, float pitch) {
    m_yaw += yaw;
    m_pitch += pitch;

    ClampPitch();
    UpdateVectors();
}

void Camera::LookAt(const XMFLOAT3& eye, const XMFLOAT3& target, const XMFLOAT3& up) {
    m_position = eye;
    m_worldUp = up;
    SetTarget(target);
}

XMFLOAT3 Camera::ScreenToWorld(const XMFLOAT2& screenPos, float depth) const {
    // Convert screen coordinates to NDC
    XMVECTOR screenVec = XMVectorSet(screenPos.x, screenPos.y, depth, 1.0f);

    // Get inverse view-projection matrix
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, GetViewProjectionMatrix());

    // Transform to world space
    XMVECTOR worldVec = XMVector4Transform(screenVec, invViewProj);

    // Perspective divide
    worldVec = XMVectorDivide(worldVec, XMVectorSplatW(worldVec));

    XMFLOAT3 result;
    XMStoreFloat3(&result, worldVec);
    return result;
}

void Camera::UpdateVectors() {
    // Calculate new forward vector
    XMFLOAT3 forward;
    forward.x = cosf(m_yaw) * cosf(m_pitch);
    forward.y = sinf(m_pitch);
    forward.z = sinf(m_yaw) * cosf(m_pitch);

    XMVECTOR forwardVec = XMVector3Normalize(XMLoadFloat3(&forward));
    XMStoreFloat3(&m_forward, forwardVec);

    // Calculate right and up vectors
    XMVECTOR worldUpVec = XMLoadFloat3(&m_worldUp);
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(forwardVec, worldUpVec));
    XMVECTOR upVec = XMVector3Normalize(XMVector3Cross(rightVec, forwardVec));

    XMStoreFloat3(&m_right, rightVec);
    XMStoreFloat3(&m_up, upVec);

    m_viewMatrixDirty = true;
}

void Camera::UpdateMatrices() const {
    if (m_viewMatrixDirty) {
        XMVECTOR posVec = XMLoadFloat3(&m_position);
        XMVECTOR forwardVec = XMLoadFloat3(&m_forward);
        XMVECTOR upVec = XMLoadFloat3(&m_up);

        XMVECTOR targetVec = XMVectorAdd(posVec, forwardVec);

        m_viewMatrix = XMMatrixLookAtRH(posVec, targetVec, upVec);
        m_viewMatrixDirty = false;
    }

    if (m_projectionMatrixDirty) {
        m_projectionMatrix = XMMatrixPerspectiveFovRH(m_fovY, m_aspectRatio, m_nearPlane, m_farPlane);
        m_projectionMatrixDirty = false;
    }
}

void Camera::ClampPitch() {
    // Prevent camera from flipping over
    const float maxPitch = XM_PIDIV2 - 0.01f; // Just under 90 degrees
    if (m_pitch > maxPitch) {
        m_pitch = maxPitch;
    }
    if (m_pitch < -maxPitch) {
        m_pitch = -maxPitch;
    }
}