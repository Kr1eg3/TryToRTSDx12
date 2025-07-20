#include "Win32Window.h"
#include "WindowsPlatform.h"

// Static members
const wchar_t* Win32Window::s_windowClassName = L"RTSGameWindowClass";
bool Win32Window::s_windowClassRegistered = false;
Win32Window* Win32Window::s_winInstance = nullptr;

Win32Window::Win32Window() {
    m_hinstance = GetModuleHandle(nullptr);

	ASSERT(s_winInstance == nullptr, "Only one Win32Window instance is allowed!");
	s_winInstance = this;
}

Win32Window::~Win32Window() {
    Destroy();

    if (s_winInstance == this) {
        s_winInstance = nullptr;
    }
}

bool Win32Window::Create(const WindowDesc& desc) {
    try {
		RegisterWindowClass();

        // Calculate window size including borders
        DWORD windowStyle = WS_OVERLAPPEDWINDOW;
        if (!desc.resizable) {
            windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        }

        RECT windowRect = { 0, 0, (LONG)desc.width, (LONG)desc.height };
        CHECK_WIN32_BOOL(AdjustWindowRect(&windowRect, windowStyle, FALSE), "AdjustWindowRect");

        int windowWidth = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;

        // Create window
        WString title = Platform::StringToWString(desc.title);
        Platform::OutputDebugMessage("Creating window with class: RTSGameWindowClass\n");
        Platform::OutputDebugMessage("Window title: " + desc.title + "\n");
        Platform::OutputDebugMessage("Window size: " + std::to_string(windowWidth) + "x" + std::to_string(windowHeight) + "\n");
        m_hwnd = CreateWindowExW(
            0,                          // Extended style
            s_windowClassName,          // Class name
            title.c_str(),             // Window title
            windowStyle,               // Window style
            CW_USEDEFAULT,             // X position
            CW_USEDEFAULT,             // Y position
            windowWidth,               // Width
            windowHeight,              // Height
            nullptr,                   // Parent window
            nullptr,                   // Menu
            m_hinstance,               // Instance handle
            0                          // Additional data
        );

        if (!m_hwnd) {
            DWORD lastError = GetLastError();
            Platform::OutputDebugMessage("CreateWindowExW failed with error: " + std::to_string(lastError) + "\n");

            // Additional debugging info
            Platform::OutputDebugMessage("Attempting to get class info...\n");
            WNDCLASSEXW classInfo = {};
            classInfo.cbSize = sizeof(WNDCLASSEXW);
            if (GetClassInfoExW(m_hinstance, s_windowClassName, &classInfo)) {
                Platform::OutputDebugMessage("Class info retrieved successfully - class exists\n");
            } else {
                DWORD classError = GetLastError();
                Platform::OutputDebugMessage("GetClassInfoExW failed with error: " + std::to_string(classError) + "\n");
            }

            THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()), "CreateWindowExW");
        }

        // Store window dimensions
        m_width = desc.width;
        m_height = desc.height;

        // Center window on screen
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int x = (screenWidth - windowWidth) / 2;
        int y = (screenHeight - windowHeight) / 2;
        SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        Platform::OutputDebugMessage("Win32Window created successfully\n");
        return true;
    }
    catch (const WindowsException& e) {
        Platform::OutputDebugMessage("Failed to create window: " + e.GetMessage());
        return false;
    }
}

void Win32Window::Destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void Win32Window::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
}

void Win32Window::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

bool Win32Window::ShouldClose() const {
    return m_shouldClose;
}

void Win32Window::PollEvents() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        // Check for WM_QUIT message
        if (msg.message == WM_QUIT) {
            Platform::OutputDebugMessage("WM_QUIT received in PollEvents\n");
            m_shouldClose = true;
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Win32Window::SetTitle(const String& title) {
    if (m_hwnd) {
        WString wTitle = Platform::StringToWString(title);
        SetWindowTextW(m_hwnd, wTitle.c_str());
    }
}

bool Win32Window::IsKeyPressed(KeyCode key) const {
    return m_keyStates[static_cast<size_t>(key)];
}

bool Win32Window::IsMouseButtonPressed(MouseButton button) const {
    return m_mouseButtonStates[static_cast<size_t>(button)];
}

void Win32Window::GetMousePosition(int32& x, int32& y) const {
    x = m_mouseX;
    y = m_mouseY;
}

LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return Win32Window::GetInstance()->HandleMessage(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            m_shouldClose = true;
            if (m_windowCloseEventCallback) {
                m_windowCloseEventCallback();
            }
            return 0;

        case WM_DESTROY:
            Platform::OutputDebugMessage("WM_DESTROY received\n");
            m_shouldClose = true;
            PostQuitMessage(0);  // This tells the message loop to exit
            return 0;

        case WM_SIZE: {
            uint32 width = LOWORD(lParam);
            uint32 height = HIWORD(lParam);

            if (width != m_width || height != m_height) {
                m_width = width;
                m_height = height;

                if (m_windowResizeEventCallback) {
                    WindowResizeEvent event{ width, height };
                    m_windowResizeEventCallback(event);
                }
            }
            return 0;
        }

        case WM_KEYDOWN:
        case WM_KEYUP: {
            KeyCode key = VirtualKeyToKeyCode(wParam);
            bool pressed = (msg == WM_KEYDOWN);
            bool repeat = (lParam & 0x40000000) != 0;

            m_keyStates[static_cast<size_t>(key)] = pressed;

            if (m_keyEventCallback) {
                KeyEvent event{ key, pressed, repeat };
                m_keyEventCallback(event);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            MouseButton button = MouseButton::Left;
            if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) button = MouseButton::Right;
            else if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP) button = MouseButton::Middle;

            bool pressed = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN);
            int32 x = GET_X_LPARAM(lParam);
            int32 y = GET_Y_LPARAM(lParam);

            m_mouseButtonStates[static_cast<size_t>(button)] = pressed;

            if (m_mouseButtonEventCallback) {
                MouseButtonEvent event{ button, pressed, x, y };
                m_mouseButtonEventCallback(event);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int32 x = GET_X_LPARAM(lParam);
            int32 y = GET_Y_LPARAM(lParam);
            int32 deltaX = x - m_lastMouseX;
            int32 deltaY = y - m_lastMouseY;

            m_mouseX = x;
            m_mouseY = y;

            if (m_mouseMoveEventCallback) {
                MouseMoveEvent event{ x, y, deltaX, deltaY };
                m_mouseMoveEventCallback(event);
            }

            m_lastMouseX = x;
            m_lastMouseY = y;
            return 0;
        }

        case WM_MOUSEWHEEL: {
            float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            int32 x = GET_X_LPARAM(lParam);
            int32 y = GET_Y_LPARAM(lParam);

            if (m_mouseWheelEventCallback) {
                MouseWheelEvent event{ delta, x, y };
                m_mouseWheelEventCallback(event);
            }
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Win32Window::RegisterWindowClass() {
    Platform::OutputDebugMessage("RegisterWindowClass() called\n");

    if (s_windowClassRegistered) {
        Platform::OutputDebugMessage("Window class already registered\n");
        return;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hinstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = s_windowClassName;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    CHECK_WIN32_BOOL(RegisterClassExW(&wc), "RegisterClassExW");
    s_windowClassRegistered = true;
}

KeyCode Win32Window::VirtualKeyToKeyCode(WPARAM vkey) const {
    // Simple mapping - extend as needed
    switch (vkey) {
        case VK_ESCAPE: return KeyCode::Escape;
        case VK_RETURN: return KeyCode::Enter;
        case VK_SPACE: return KeyCode::Space;
        case VK_TAB: return KeyCode::Tab;
        case VK_SHIFT: return KeyCode::Shift;
        case VK_CONTROL: return KeyCode::Ctrl;
        case VK_MENU: return KeyCode::Alt;
        case VK_LEFT: return KeyCode::Left;
        case VK_UP: return KeyCode::Up;
        case VK_RIGHT: return KeyCode::Right;
        case VK_DOWN: return KeyCode::Down;
        case VK_F1: return KeyCode::F1;
        case VK_F2: return KeyCode::F2;
        case VK_F3: return KeyCode::F3;
        case VK_F4: return KeyCode::F4;
        case VK_F5: return KeyCode::F5;
        case VK_F6: return KeyCode::F6;
        case VK_F7: return KeyCode::F7;
        case VK_F8: return KeyCode::F8;
        case VK_F9: return KeyCode::F9;
        case VK_F10: return KeyCode::F10;
        case VK_F11: return KeyCode::F11;
        case VK_F12: return KeyCode::F12;
        default:
            if (vkey >= 'A' && vkey <= 'Z') {
                return static_cast<KeyCode>(vkey);
            }
            if (vkey >= '0' && vkey <= '9') {
                return static_cast<KeyCode>(vkey);
            }
            return KeyCode::Unknown;
    }
}

// Factory method implementation
UniquePtr<Window> Window::Create() {
    return std::make_unique<Win32Window>();
}