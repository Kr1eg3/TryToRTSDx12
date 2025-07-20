#include "Application.h"
#include "../../Platform/Windows/WindowsPlatform.h"

// Static instance
Application* Application::s_instance = nullptr;

Application::Application(const ApplicationConfig& config)
    : m_config(config)
{
    ASSERT(s_instance == nullptr, "Application instance already exists!");
    s_instance = this;
}

Application::~Application() {
    if (m_initialized) {
        Shutdown();
    }
    s_instance = nullptr;
}

bool Application::Initialize() {
    if (m_initialized) {
        return true;
    }

    Platform::OutputDebugMessage("Initializing application: " + m_config.name + "\n");

    // Create window
    if (!CreateAppWindow()) {
        Platform::OutputDebugMessage("Failed to create window\n");
        return false;
    }

    // Create renderer
    if (!CreateRenderer()) {
        Platform::OutputDebugMessage("Failed to create renderer\n");
        return false;
    }

    // Setup event callbacks
    SetupEventCallbacks();

    // Initialize timer
    m_timer.Reset();
    m_timer.Start();

    // Call derived class initialization
    if (!OnInitialize()) {
        Platform::OutputDebugMessage("Derived class initialization failed\n");
        return false;
    }

    m_initialized = true;
    Platform::OutputDebugMessage("Application initialized successfully\n");
    return true;
}

int32 Application::Run() {
    if (!m_initialized) {
        Platform::OutputDebugMessage("Application not initialized\n");
        return -1;
    }

    Platform::OutputDebugMessage("Starting main loop\n");

    // Show window
    m_window->Show();

    // Main loop
    MainLoop();

    Platform::OutputDebugMessage("Main loop ended\n");
    return 0;
}

void Application::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Platform::OutputDebugMessage("Shutting down application\n");

    // Call derived class shutdown
    OnShutdown();

    // Cleanup renderer before window
    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    // Cleanup window
    if (m_window) {
        m_window->Destroy();
        m_window.reset();
    }

    m_initialized = false;
    Platform::OutputDebugMessage("Application shutdown complete\n");
}

bool Application::CreateAppWindow() {
    m_window = Window::Create();
    if (!m_window) {
        return false;
    }

    return m_window->Create(m_config.windowDesc);
}

bool Application::CreateRenderer() {
    m_renderer = Renderer::Create();
    if (!m_renderer) {
        Platform::OutputDebugMessage("Failed to create renderer instance\n");
        return false;
    }

    if (!m_renderer->Initialize(m_window.get(), m_config.rendererConfig)) {
        Platform::OutputDebugMessage("Failed to initialize renderer\n");
        return false;
    }

    return true;
}

void Application::SetupEventCallbacks() {
    m_window->SetWindowResizeEventCallback(
        [this](const WindowResizeEvent& event) {
            HandleWindowResize(event);
        }
    );

    m_window->SetWindowCloseEventCallback(
        [this]() {
            HandleWindowClose();
        }
    );

    m_window->SetKeyEventCallback(
        [this](const KeyEvent& event) {
            HandleKeyEvent(event);
        }
    );

    m_window->SetMouseButtonEventCallback(
        [this](const MouseButtonEvent& event) {
            HandleMouseButtonEvent(event);
        }
    );

    m_window->SetMouseMoveEventCallback(
        [this](const MouseMoveEvent& event) {
            HandleMouseMoveEvent(event);
        }
    );

    m_window->SetMouseWheelEventCallback(
        [this](const MouseWheelEvent& event) {
            HandleMouseWheelEvent(event);
        }
    );
}

void Application::MainLoop() {
    Platform::OutputDebugMessage("Entering main loop\n");

    while (!m_shouldExit && !m_window->ShouldClose()) {
        // Poll window events
        m_window->PollEvents();

        // Double-check if window should close after polling events
        if (m_window->ShouldClose()) {
            Platform::OutputDebugMessage("Window should close detected in main loop\n");
            break;
        }

        // Update timer
        m_timer.Tick();

        // Update and render
        Update();
        Render();
    }

    Platform::OutputDebugMessage("Exiting main loop - shouldExit: " +
                                std::to_string(m_shouldExit) +
                                ", windowShouldClose: " +
                                std::to_string(m_window->ShouldClose()) + "\n");
}

void Application::Update() {
    float32 deltaTime = m_timer.GetDeltaTime();

    // Call derived class update
    OnUpdate(deltaTime);
}

void Application::Render() {
    // Begin frame
    m_renderer->BeginFrame();

    // Clear screen
    ClearValues clearValues;
    clearValues.color = { 0.2f, 0.3f, 0.4f, 1.0f }; // Nice blue-gray
    m_renderer->Clear(clearValues);

    // Call derived class render
    OnRender();

    // End frame and present
    m_renderer->EndFrame();
    m_renderer->Present();
}

void Application::HandleWindowResize(const WindowResizeEvent& event) {
    Platform::OutputDebugMessage("Window resize: " +
        std::to_string(event.width) + "x" + std::to_string(event.height) + "\n");

    // Resize renderer
    if (m_renderer) {
        m_renderer->Resize(event.width, event.height);
    }

    OnWindowResize(event.width, event.height);

}

void Application::HandleWindowClose() {
    Platform::OutputDebugMessage("Window close requested\n");
    m_shouldExit = true;
}

void Application::HandleKeyEvent(const KeyEvent& event) {
    OnKeyEvent(event);
}

void Application::HandleMouseButtonEvent(const MouseButtonEvent& event) {
    OnMouseButtonEvent(event);
}

void Application::HandleMouseMoveEvent(const MouseMoveEvent& event) {
    OnMouseMoveEvent(event);
}

void Application::HandleMouseWheelEvent(const MouseWheelEvent& event) {
    OnMouseWheelEvent(event);
}