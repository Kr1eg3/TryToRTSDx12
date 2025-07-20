#pragma once

#include "../../Core/Utilities/Types.h"
#include <functional>

// Input enums
enum class KeyCode : uint16 {
    Unknown = 0,

    // Letters
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Numbers
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1 = 112, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Arrow keys
    Left = 37, Up, Right, Down,

    // Special keys
    Escape = 27,
    Enter = 13,
    Space = 32,
    Tab = 9,
    Shift = 16,
    Ctrl = 17,
    Alt = 18
};

enum class MouseButton : uint8 {
    Left = 0,
    Right = 1,
    Middle = 2
};

// Input event structures
struct KeyEvent {
    KeyCode key;
    bool pressed;
    bool repeat;
};

struct MouseButtonEvent {
    MouseButton button;
    bool pressed;
    int32 x, y;
};

struct MouseMoveEvent {
    int32 x, y;
    int32 deltaX, deltaY;
};

struct MouseWheelEvent {
    float delta;
    int32 x, y;
};

struct WindowResizeEvent {
    uint32 width, height;
};

// Event callbacks
using KeyEventCallback = Function<void(const KeyEvent&)>;
using MouseButtonEventCallback = Function<void(const MouseButtonEvent&)>;
using MouseMoveEventCallback = Function<void(const MouseMoveEvent&)>;
using MouseWheelEventCallback = Function<void(const MouseWheelEvent&)>;
using WindowResizeEventCallback = Function<void(const WindowResizeEvent&)>;
using WindowCloseEventCallback = Function<void()>;

// Window description
struct WindowDesc {
    String title = "RTS Game";
    uint32 width = 1280;
    uint32 height = 720;
    bool fullscreen = false;
    bool resizable = true;
    bool vsync = true;
};

// Abstract window interface
class Window {
public:
    virtual ~Window() = default;

    // Core window operations
    virtual bool Create(const WindowDesc& desc) = 0;
    virtual void Destroy() = 0;
    virtual void Show() = 0;
    virtual void Hide() = 0;
    virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;

    // Window properties
    virtual uint32 GetWidth() const = 0;
    virtual uint32 GetHeight() const = 0;
    virtual void SetTitle(const String& title) = 0;
    virtual void* GetNativeHandle() const = 0;

    // Input state queries
    virtual bool IsKeyPressed(KeyCode key) const = 0;
    virtual bool IsMouseButtonPressed(MouseButton button) const = 0;
    virtual void GetMousePosition(int32& x, int32& y) const = 0;

    // Event callbacks
    virtual void SetKeyEventCallback(KeyEventCallback callback) = 0;
    virtual void SetMouseButtonEventCallback(MouseButtonEventCallback callback) = 0;
    virtual void SetMouseMoveEventCallback(MouseMoveEventCallback callback) = 0;
    virtual void SetMouseWheelEventCallback(MouseWheelEventCallback callback) = 0;
    virtual void SetWindowResizeEventCallback(WindowResizeEventCallback callback) = 0;
    virtual void SetWindowCloseEventCallback(WindowCloseEventCallback callback) = 0;

    // Factory method
    static UniquePtr<Window> Create();

protected:
    Window() = default;
    DECLARE_NON_COPYABLE(Window);
};