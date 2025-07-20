#pragma once

#include "../../Core/Window/Window.h"
#include "WindowsPlatform.h"
#include <array>

class Win32Window : public Window {
public:
    Win32Window();
    virtual ~Win32Window();

    // Window interface implementation
    bool Create(const WindowDesc& desc) override;
    void Destroy() override;
    void Show() override;
    void Hide() override;
    bool ShouldClose() const override;
    void PollEvents() override;

    // Window properties
    uint32 GetWidth() const override { return m_width; }
    uint32 GetHeight() const override { return m_height; }
    void SetTitle(const String& title) override;
    void* GetNativeHandle() const override { return m_hwnd; }

    // Input state queries
    bool IsKeyPressed(KeyCode key) const override;
    bool IsMouseButtonPressed(MouseButton button) const override;
    void GetMousePosition(int32& x, int32& y) const override;

    // Event callbacks
    void SetKeyEventCallback(KeyEventCallback callback) override { m_keyEventCallback = callback; }
    void SetMouseButtonEventCallback(MouseButtonEventCallback callback) override { m_mouseButtonEventCallback = callback; }
    void SetMouseMoveEventCallback(MouseMoveEventCallback callback) override { m_mouseMoveEventCallback = callback; }
    void SetMouseWheelEventCallback(MouseWheelEventCallback callback) override { m_mouseWheelEventCallback = callback; }
    void SetWindowResizeEventCallback(WindowResizeEventCallback callback) override { m_windowResizeEventCallback = callback; }
    void SetWindowCloseEventCallback(WindowCloseEventCallback callback) override { m_windowCloseEventCallback = callback; }

	static Win32Window* GetInstance() { return s_winInstance; }

private:
    // Windows message handling
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Helper methods
    void RegisterWindowClass();
    KeyCode VirtualKeyToKeyCode(WPARAM vkey) const;

protected:
	static Win32Window* s_winInstance;

private:

    // Window data
    HWND m_hwnd = nullptr;
    HINSTANCE m_hinstance = nullptr;
    uint32 m_width = 0;
    uint32 m_height = 0;
    bool m_shouldClose = false;

    // Input state
    std::array<bool, 256> m_keyStates{};
    std::array<bool, 3> m_mouseButtonStates{};
    int32 m_mouseX = 0;
    int32 m_mouseY = 0;
    int32 m_lastMouseX = 0;
    int32 m_lastMouseY = 0;

    // Event callbacks
    KeyEventCallback m_keyEventCallback;
    MouseButtonEventCallback m_mouseButtonEventCallback;
    MouseMoveEventCallback m_mouseMoveEventCallback;
    MouseWheelEventCallback m_mouseWheelEventCallback;
    WindowResizeEventCallback m_windowResizeEventCallback;
    WindowCloseEventCallback m_windowCloseEventCallback;

    // Window class
    static const wchar_t* s_windowClassName;
    static bool s_windowClassRegistered;
};