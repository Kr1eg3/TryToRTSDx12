#include "Source/Core/Application/Application.h"
#include "Source/Platform/Windows/WindowsPlatform.h"
#include "Source/Core/Scene/Scene.h"
#include "Source/Core/Entity/Entity.h"
#include "Source/Core/Entity/MeshComponent.h"
#include "Source/Core/Entity/TransformComponent.h"
#include "Source/Rendering/Dx12/DX12Renderer.h"
#include "Source/Rendering/ShaderManager.h"
#include <DirectXMath.h>

class GameScene : public Scene {
private:
    UniquePtr<ShaderManager> m_shaderManager;
    Entity* m_cubeEntity = nullptr;
    Entity* m_secondCube = nullptr;
    float m_rotationSpeed = 1.0f;

public:
    GameScene() {
        SetName("Game Scene");
    }

    void Initialize() override {
        Platform::OutputDebugMessage("GameScene: Initializing...\n");

        Scene::Initialize();

        Platform::OutputDebugMessage("GameScene: Initialized successfully\n");
    }

    bool InitializeShaders(DX12Renderer* renderer) {
        Platform::OutputDebugMessage("GameScene: Initializing shaders...\n");

        m_shaderManager = std::make_unique<ShaderManager>();
        if (!m_shaderManager->Initialize(renderer)) {
            Platform::OutputDebugMessage("GameScene: Failed to initialize shader manager\n");
            return false;
        }

        SetShaderManager(m_shaderManager.get());

        Platform::OutputDebugMessage("GameScene: Shaders initialized successfully\n");
        return true;
    }

    void BeginPlay() override {
        Platform::OutputDebugMessage("GameScene: Begin play...\n");

        Scene::BeginPlay();

        m_cubeEntity = SpawnEntity<Entity>();
        m_cubeEntity->SetName("Rotating Cube");

        auto transform = m_cubeEntity->GetComponent<TransformComponent>();
        transform->SetPosition(0.0f, 0.0f, 0.0f);
        transform->SetScale(1.0f);

        auto meshComp = m_cubeEntity->AddComponent<MeshComponent>();

        m_secondCube = SpawnEntity<Entity>();
        m_secondCube->SetName("Static Cube");

        auto transform2 = m_secondCube->GetComponent<TransformComponent>();
        transform2->SetPosition(4.0f, 0.0f, 0.0f);
        transform2->SetScale(0.8f);

        auto meshComp2 = m_secondCube->AddComponent<MeshComponent>();

        Platform::OutputDebugMessage("GameScene: Entities created successfully\n");
    }

    bool SetupMeshes(DX12Renderer* renderer) {
        Platform::OutputDebugMessage("GameScene: Setting up meshes...\n");

        for (auto& entity : GetEntities()) {
            auto meshComp = entity->GetComponent<MeshComponent>();
            if (meshComp) {
                if (!meshComp->CreateCube(renderer)) {
                    Platform::OutputDebugMessage("GameScene: Failed to create cube mesh\n");
                    return false;
                }
            }
        }

        Platform::OutputDebugMessage("GameScene: Meshes setup successfully\n");
        return true;
    }

    void Update(float deltaTime) override {
        if (m_cubeEntity && m_cubeEntity->IsActive()) {
            auto transform = m_cubeEntity->GetComponent<TransformComponent>();
            if (transform) {
                float currentY = transform->GetRotation().y;
                transform->SetRotation(0.0f, currentY + deltaTime * m_rotationSpeed, 0.0f);
            }
        }

        if (m_secondCube && m_secondCube->IsActive()) {
            auto transform = m_secondCube->GetComponent<TransformComponent>();
            if (transform) {
                DirectX::XMFLOAT3 currentRot = transform->GetRotation();
                transform->SetRotation(
                    currentRot.x + deltaTime * 0.3f,
                    currentRot.y,
                    currentRot.z + deltaTime * 0.5f
                );
            }
        }

        Scene::Update(deltaTime);
    }

    void Render(DX12Renderer* renderer) override {
        if (!renderer || !m_shaderManager) return;

        UploadMeshData(renderer);

        UpdateLightConstants();

        Scene::Render(renderer);
    }

    void OnEntitySpawned(Entity* entity) override {
        Platform::OutputDebugMessage("GameScene: Entity spawned - " + entity->GetName() + "\n");
    }

    void OnEntityDestroyed(Entity* entity) override {
        Platform::OutputDebugMessage("GameScene: Entity destroyed - " + entity->GetName() + "\n");
    }

private:
    void UploadMeshData(DX12Renderer* renderer) {
        for (auto& entity : GetEntities()) {
            auto meshComp = entity->GetComponent<MeshComponent>();
            if (meshComp && meshComp->HasMesh()) {
                auto mesh = meshComp->GetMesh();
                if (mesh && mesh->NeedsUpload()) {
                    mesh->UploadData(renderer);
                }
            }
        }
    }

    void UpdateLightConstants() {
        DirectX::XMFLOAT3 lightDirection = { 0.3f, -0.7f, 0.6f };
        DirectX::XMFLOAT3 lightColor = { 1.0f, 0.95f, 0.8f };
        float lightIntensity = 1.2f;

        m_shaderManager->UpdateLightConstants(lightDirection, lightColor, lightIntensity);
    }
};

class RTSApplication : public Application {
private:
    UniquePtr<GameScene> m_gameScene;

public:
    RTSApplication() : Application(CreateConfig()) {
    }

private:
    static ApplicationConfig CreateConfig() {
        ApplicationConfig config;
        config.name = "RTS Game - Entity System";
        config.windowDesc.title = "RTS Game - Entity System Demo";
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

        // Camera settings - позиция для лучшего обзора двух кубов
        config.cameraDesc.position = { 6.0f, 4.0f, -8.0f };
        config.cameraDesc.target = { 2.0f, 0.0f, 0.0f };
        config.cameraDesc.fovY = DirectX::XM_PIDIV4;
        config.cameraDesc.moveSpeed = 12.0f;
        config.cameraDesc.mouseSensitivity = 0.002f;
        config.cameraDesc.scrollSensitivity = 2.5f;

        return config;
    }

protected:
    bool OnInitialize() override {
        Platform::OutputDebugMessage("RTSApplication: Initializing...\n");

        // Получаем DX12 renderer
        DX12Renderer* dx12Renderer = static_cast<DX12Renderer*>(GetRenderer());
        if (!dx12Renderer) {
            Platform::OutputDebugMessage("RTSApplication: Failed to get DX12 renderer\n");
            return false;
        }

        m_gameScene = std::make_unique<GameScene>();

        m_gameScene->Initialize();

        if (!m_gameScene->InitializeShaders(dx12Renderer)) {
            Platform::OutputDebugMessage("RTSApplication: Failed to initialize scene shaders\n");
            return false;
        }

        m_gameScene->BeginPlay();

        if (!m_gameScene->SetupMeshes(dx12Renderer)) {
            Platform::OutputDebugMessage("RTSApplication: Failed to setup scene meshes\n");
            return false;
        }

        Platform::OutputDebugMessage("RTSApplication: Initialized successfully!\n");
        Platform::OutputDebugMessage("Controls:\n");
        Platform::OutputDebugMessage("  WASD - Move camera\n");
        Platform::OutputDebugMessage("  Right mouse + drag - Look around\n");
        Platform::OutputDebugMessage("  Mouse wheel - Zoom\n");
        Platform::OutputDebugMessage("  R - Reset camera\n");
        Platform::OutputDebugMessage("  ESC - Exit\n");

        return true;
    }

    void OnShutdown() override {
        Platform::OutputDebugMessage("RTSApplication: Shutting down...\n");

        if (m_gameScene) {
            m_gameScene->EndPlay();
            m_gameScene.reset();
        }
    }

    void OnUpdate(float32 deltaTime) override {
        if (m_gameScene) {
            m_gameScene->Update(deltaTime);
        }

        static float32 fpsTimer = 0.0f;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            String windowTitle = "RTS Game - Entity System | FPS: " +
                               std::to_string(static_cast<int>(GetTimer().GetFPS())) +
                               " | Entities: " + std::to_string(m_gameScene->GetEntityCount());
            GetWindow()->SetTitle(windowTitle);
            fpsTimer = 0.0f;
        }
    }

    void OnRender() override {
        DX12Renderer* dx12Renderer = static_cast<DX12Renderer*>(GetRenderer());
        if (!dx12Renderer) return;

        if (m_gameScene && m_gameScene->GetShaderManager() && GetCamera()) {
            DirectX::XMMATRIX viewMatrix = GetCamera()->GetViewMatrix();
            DirectX::XMMATRIX projMatrix = GetCamera()->GetProjectionMatrix();
            DirectX::XMFLOAT3 cameraPosition = GetCamera()->GetPosition();

            m_gameScene->GetShaderManager()->UpdateViewConstants(viewMatrix, projMatrix, cameraPosition);
        }

        if (m_gameScene) {
            m_gameScene->Render(dx12Renderer);
        }
    }

    void OnKeyEvent(const KeyEvent& event) override {
        if (event.pressed) {
            switch (event.key) {
                case KeyCode::Escape:
                    Platform::OutputDebugMessage("Escape pressed, exiting...\n");
                    RequestExit();
                    break;

                case KeyCode::F1:
                    if (m_gameScene) {
                        Platform::OutputDebugMessage("Scene entities: " +
                                                    std::to_string(m_gameScene->GetEntityCount()) + "\n");
                    }
                    break;

                case KeyCode::F2:
                    if (m_gameScene) {
                        auto newEntity = m_gameScene->SpawnEntity<Entity>();
                        newEntity->SetName("Runtime Cube " + std::to_string(m_gameScene->GetEntityCount()));

                        auto transform = newEntity->GetComponent<TransformComponent>();
                        float x = static_cast<float>((rand() % 10) - 5);
                        float z = static_cast<float>((rand() % 10) - 5);
                        transform->SetPosition(x, 1.0f, z);
                        transform->SetScale(0.5f);

                        auto meshComp = newEntity->AddComponent<MeshComponent>();
                        DX12Renderer* renderer = static_cast<DX12Renderer*>(GetRenderer());
                        meshComp->CreateCube(renderer);

                        Platform::OutputDebugMessage("Created new entity at runtime\n");
                    }
                    break;
            }
        }
    }

    void OnWindowResize(uint32 width, uint32 height) override {
        Platform::OutputDebugMessage("Window resized to " +
            std::to_string(width) + "x" + std::to_string(height) + "\n");
    }
};

// Entry point
int CALLBACK WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    try {
        RTSApplication app;
        if (!app.Initialize()) {
            Platform::ShowMessageBox("Error", "Failed to initialize application");
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
}