#pragma once

#include "../Core/Utilities/Types.h"
#include <DirectXMath.h>

// Forward declarations
struct KeyEvent;
struct MouseButtonEvent;
struct MouseMoveEvent;
struct MouseWheelEvent;

// Camera configuration
struct CameraDesc {
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, -5.0f };
    DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

    float fovY = DirectX::XM_PIDIV4;  // 45 degrees
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // Movement settings
    float moveSpeed = 10.0f;          // units per second
    float rotationSpeed = 2.0f;       // radians per second
    float mouseSensitivity = 0.003f;  // radians per pixel
    float scrollSensitivity = 2.0f;   // units per scroll tick
};

// Camera class
class Camera {
public:
    Camera(const CameraDesc& desc = {});
    ~Camera() = default;

    // Update camera (call every frame)
    void Update(float32 deltaTime);

    // Input handling
    void OnKeyEvent(const KeyEvent& event);
    void OnMouseButtonEvent(const MouseButtonEvent& event);
    void OnMouseMoveEvent(const MouseMoveEvent& event);
    void OnMouseWheelEvent(const MouseWheelEvent& event);

    // Matrix getters
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;
    DirectX::XMMATRIX GetViewProjectionMatrix() const;

    // Camera properties
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }
    DirectX::XMFLOAT3 GetForward() const { return m_forward; }
    DirectX::XMFLOAT3 GetRight() const { return m_right; }
    DirectX::XMFLOAT3 GetUp() const { return m_up; }

    // Setters
    void SetPosition(const DirectX::XMFLOAT3& position);
    void SetTarget(const DirectX::XMFLOAT3& target);
    void SetAspectRatio(float aspectRatio);
    void SetFOV(float fovY);
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }

    // Camera controls
    void MoveForward(float distance);
    void MoveRight(float distance);
    void MoveUp(float distance);
    void Rotate(float yaw, float pitch);

    // Utilities
    void LookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);
    DirectX::XMFLOAT3 ScreenToWorld(const DirectX::XMFLOAT2& screenPos, float depth = 1.0f) const;

private:
    // Update internal vectors
    void UpdateVectors();
    void UpdateMatrices() const;

    // Clamp pitch to avoid gimbal lock
    void ClampPitch();

private:
    // Camera vectors
    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_forward;
    DirectX::XMFLOAT3 m_right;
    DirectX::XMFLOAT3 m_up;
    DirectX::XMFLOAT3 m_worldUp;

    // Euler angles (in radians)
    float m_yaw = -DirectX::XM_PIDIV2;    // -90 degrees (looking at +Z)
    float m_pitch = 0.0f;

    // Projection settings
    float m_fovY;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;

    // Movement settings
    float m_moveSpeed;
    float m_rotationSpeed;
    float m_mouseSensitivity;
    float m_scrollSensitivity;

    // Input state
    bool m_keys[256] = {};
    bool m_mouseButtons[3] = {};
    bool m_firstMouse = true;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;

    // Cached matrices
    mutable DirectX::XMMATRIX m_viewMatrix;
    mutable DirectX::XMMATRIX m_projectionMatrix;
    mutable bool m_viewMatrixDirty = true;
    mutable bool m_projectionMatrixDirty = true;

    DECLARE_NON_COPYABLE(Camera);
};