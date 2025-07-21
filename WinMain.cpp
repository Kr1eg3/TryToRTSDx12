#include "Source/Core/Application/Application.h"
#include "Source/Platform/Windows/WindowsPlatform.h"
#include "Source/Rendering/Mesh.h"
#include "Source/Rendering/ShaderManager.h"
#include "Source/Rendering/Dx12/DX12Renderer.h"
#include <DirectXMath.h>

// Simple RTS application class
class RTSApplication : public Application {
private:
    // Game objects
    UniquePtr<Mesh> m_cube;
    UniquePtr<ShaderManager> m_shaderManager;

    // Camera and view matrices
    DirectX::XMMATRIX m_viewMatrix;
    DirectX::XMMATRIX m_projectionMatrix;
    DirectX::XMFLOAT3 m_cameraPosition;
    float m_rotationY = 0.0f;

public:
    RTSApplication() : Application(CreateConfig()) {
    }

private:
    static ApplicationConfig CreateConfig() {
        ApplicationConfig config;
        config.name = "RTS Game - DirectX 12";
        config.windowDesc.title = "RTS Game - DirectX 12";
        config.windowDesc.width = 1280;
        config.windowDesc.height = 720;
        config.windowDesc.resizable = true;
        config.windowDesc.vsync = true;

        // Renderer settings
        config.rendererConfig.enableDebugLayer = DEBUG_BUILD;
        config.rendererConfig.enableGpuValidation = DEBUG_BUILD;
        config.rendererConfig.enableBreakOnError = DEBUG_BUILD;
        config.rendererConfig.backBufferCount = 2;
        config.rendererConfig.vsyncEnabled = true;
        config.rendererConfig.maxFramesInFlight = 2;
        config.rendererConfig.gpuMemoryBudgetMB = 512;

        return config;
    }

protected:
    bool OnInitialize() override {
        Platform::OutputDebugMessage("RTS Application initializing...\n");

        // Get DX12 renderer
        DX12Renderer* dx12Renderer = static_cast<DX12Renderer*>(GetRenderer());
        if (!dx12Renderer) {
            Platform::OutputDebugMessage("Failed to get DX12 renderer\n");
            return false;
        }

        // Initialize shader manager
        m_shaderManager = std::make_unique<ShaderManager>();
        if (!m_shaderManager->Initialize(dx12Renderer)) {
            Platform::OutputDebugMessage("Failed to initialize shader manager\n");
            return false;
        }

        // Create cube mesh
        m_cube = std::make_unique<Mesh>();
        if (!m_cube->CreateCube(dx12Renderer)) {
            Platform::OutputDebugMessage("Failed to create cube mesh\n");
            return false;
        }

        // Setup camera
        m_cameraPosition = { 0.0f, 0.0f, -5.0f };
        m_viewMatrix = DirectX::XMMatrixLookAtLH(
            DirectX::XMLoadFloat3(&m_cameraPosition),
            DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        );

        // Setup projection matrix
        float aspectRatio = static_cast<float>(GetWindow()->GetWidth()) / static_cast<float>(GetWindow()->GetHeight());
        m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(45.0f),
            aspectRatio,
            0.1f,
            100.0f
        );

        Platform::OutputDebugMessage("RTS Application initialized successfully!\n");
        return true;
    }

    void OnShutdown() override {
        Platform::OutputDebugMessage("RTS Application shutting down...\n");

        // Cleanup in reverse order
        m_cube.reset();
        m_shaderManager.reset();
    }

    void OnUpdate(float32 deltaTime) override {
        // Rotate the cube
        m_rotationY += deltaTime * 0.5f; // Half a radian per second

        // Example: Print FPS every second
        static float32 fpsTimer = 0.0f;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            String fpsText = "FPS: " + std::to_string(GetTimer().GetFPS());
            Platform::OutputDebugMessage(fpsText + "\n");
            fpsTimer = 0.0f;
        }
    }

    void OnRender() override {
        // Get DX12 renderer
        DX12Renderer* dx12Renderer = static_cast<DX12Renderer*>(GetRenderer());
        if (!dx12Renderer) return;

        // Upload mesh data if needed (this should be done when command list is recording)
        if (m_cube && m_cube->NeedsUpload()) {
            m_cube->UploadData(dx12Renderer);
        }

        // Update shader constants
        if (m_shaderManager) {
            // Model matrix (rotation around Y axis)
            DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixRotationY(m_rotationY);
            m_shaderManager->UpdateModelConstants(modelMatrix);

            // View and projection matrices
            m_shaderManager->UpdateViewConstants(m_viewMatrix, m_projectionMatrix, m_cameraPosition);

            // Light constants
            DirectX::XMFLOAT3 lightDirection = { 0.0f, -1.0f, 1.0f };
            DirectX::XMFLOAT3 lightColor = { 1.0f, 1.0f, 1.0f };
            float lightIntensity = 1.0f;
            m_shaderManager->UpdateLightConstants(lightDirection, lightColor, lightIntensity);

            // Bind shaders and resources
            m_shaderManager->BindForMeshRendering(dx12Renderer->GetCommandList());
        }

        // Draw cube
        if (m_cube) {
            m_cube->Draw(dx12Renderer->GetCommandList());
        }
    }

    void OnWindowResize(uint32 width, uint32 height) override {
        Platform::OutputDebugMessage("Window resized to " +
            std::to_string(width) + "x" + std::to_string(height) + "\n");

        // Update projection matrix for new aspect ratio
        float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(45.0f),
            aspectRatio,
            0.1f,
            100.0f
        );
    }

    void OnKeyEvent(const KeyEvent& event) override {
        if (event.key == KeyCode::Escape && event.pressed) {
            Platform::OutputDebugMessage("Escape pressed, requesting exit...\n");
            RequestExit();
        }

        if (event.key == KeyCode::F11 && event.pressed) {
            Platform::OutputDebugMessage("F11 pressed - Toggle fullscreen (not implemented yet)\n");
        }
    }

    void OnMouseButtonEvent(const MouseButtonEvent& event) override {
        String buttonName = (event.button == MouseButton::Left) ? "Left" :
                           (event.button == MouseButton::Right) ? "Right" : "Middle";
        String action = event.pressed ? "pressed" : "released";

        Platform::OutputDebugMessage("Mouse " + buttonName + " " + action +
            " at (" + std::to_string(event.x) + ", " + std::to_string(event.y) + ")\n");
    }

    void OnMouseMoveEvent(const MouseMoveEvent& event) override {
        // Uncomment for mouse tracking (will be very verbose)
        /*
        Platform::OutputDebugMessage("Mouse moved to (" +
            std::to_string(event.x) + ", " + std::to_string(event.y) +
            ") delta: (" + std::to_string(event.deltaX) + ", " +
            std::to_string(event.deltaY) + ")\n");
        */
    }

    void OnMouseWheelEvent(const MouseWheelEvent& event) override {
        Platform::OutputDebugMessage("Mouse wheel: " + std::to_string(event.delta) +
            " at (" + std::to_string(event.x) + ", " + std::to_string(event.y) + ")\n");
    }
};

//IMPLEMENT_APPLICATION(RTSApplication)
int CALLBACK WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    try {
        RTSApplication app;
        if (!app.Initialize()) {
            return -1;
        }
        return app.Run();
    }
    catch (const std::exception& e) {
        Platform::ShowMessageBox("Error", e.what());
        return -1;
    }
    catch (...) {
        Platform::ShowMessageBox("Error", "Unknown error occurred");
        return -1;
    }

	return 0;
}