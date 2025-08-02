#include "Source/Core/Application/Application.h"
#include "Source/Platform/Windows/WindowsPlatform.h"
#include "Source/Core/Scene/Scene.h"
#include "Source/Core/Entity/Entity.h"
#include "Source/Core/Entity/MeshComponent.h"
#include "Source/Core/Entity/TransformComponent.h"
#include "Source/Rendering/Dx12/DX12Renderer.h"
#include "Source/Rendering/Material.h"
#include "Source/Rendering/Bindable/Texture.h"
#include <DirectXMath.h>

class GameScene : public Scene {
private:
    Entity* m_cubeEntity = nullptr;
    Entity* m_secondCube = nullptr;
    Entity* m_lightSphere = nullptr;
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


    void BeginPlay() override {
        Platform::OutputDebugMessage("GameScene: Begin play...\n");

        Scene::BeginPlay();

        // Create first textured cube with bricks.dds
        m_cubeEntity = SpawnEntity<Entity>();
        m_cubeEntity->SetName("Textured Cube (bricks.dds)");

        auto transform = m_cubeEntity->GetComponent<TransformComponent>();
        transform->SetPosition(0.0f, 0.0f, 0.0f);
        transform->SetScale(1.0f);

        auto meshComp = m_cubeEntity->AddComponent<MeshComponent>();

        // Create second textured cube with bricks2.dds
        m_secondCube = SpawnEntity<Entity>();
        m_secondCube->SetName("Textured Cube (bricks2.dds)");

        auto transform2 = m_secondCube->GetComponent<TransformComponent>();
        transform2->SetPosition(4.0f, 0.0f, 0.0f);
        transform2->SetScale(0.8f);

        auto meshComp2 = m_secondCube->AddComponent<MeshComponent>();

        // Create light sphere at light position (5.0f, 8.0f, -3.0f)
        m_lightSphere = SpawnEntity<Entity>();
        m_lightSphere->SetName("Light Source Sphere");

        auto lightTransform = m_lightSphere->GetComponent<TransformComponent>();
        lightTransform->SetPosition(5.0f, 8.0f, -3.0f);
        lightTransform->SetScale(0.3f); // Small sphere

        auto lightMeshComp = m_lightSphere->AddComponent<MeshComponent>();

        Platform::OutputDebugMessage("GameScene: Textured entities and light sphere created successfully\n");
    }

    bool SetupMeshes(DX12Renderer* renderer) {
        Platform::OutputDebugMessage("GameScene: Setting up meshes...\n");

        // Create cube meshes for all entities
        for (auto& entity : GetEntities()) {
            auto meshComp = entity->GetComponent<MeshComponent>();
            if (meshComp) {
                if (!meshComp->CreateCube(renderer)) {
                    Platform::OutputDebugMessage("GameScene: Failed to create cube mesh\n");
                    return false;
                }
            }
        }

        // Create simple colored materials without textures for now
        if (m_cubeEntity) {
            auto meshComp = m_cubeEntity->GetComponent<MeshComponent>();
            if (meshComp) {
                // Create red unlit material for first cube
                auto material = Material::CreateUnlit(*renderer, {1.0f, 0.0f, 0.0f, 1.0f}, "RedMaterial");
                meshComp->SetMaterial(material);
                Platform::OutputDebugMessage("Applied red material to first cube\n");
            }
        }

        if (m_secondCube) {
            auto meshComp2 = m_secondCube->GetComponent<MeshComponent>();
            if (meshComp2) {
                // Create blue unlit material for second cube
                auto material = Material::CreateUnlit(*renderer, {0.0f, 0.0f, 1.0f, 1.0f}, "BlueMaterial");
                meshComp2->SetMaterial(material);
                Platform::OutputDebugMessage("Applied blue material to second cube\n");
            }
        }

        // Setup light sphere
        if (m_lightSphere) {
            auto lightMeshComp = m_lightSphere->GetComponent<MeshComponent>();
            if (lightMeshComp) {
                // Create sphere mesh
                if (!lightMeshComp->CreateSphere(renderer, 12, 16)) {
                    Platform::OutputDebugMessage("GameScene: Failed to create light sphere mesh\n");
                    return false;
                }
                
                // Create material with light color (1.0f, 0.95f, 0.8f) - warm white
                auto material = Material::CreateUnlit(*renderer, {1.0f, 0.95f, 0.8f, 1.0f}, "LightMaterial");
                lightMeshComp->SetMaterial(material);
                Platform::OutputDebugMessage("Applied light-colored material to light sphere\n");
            }
        }

        // Now force upload all textures
        Platform::OutputDebugMessage("GameScene: Force uploading all textures...\n");
        UploadTextureData(renderer);

        Platform::OutputDebugMessage("GameScene: Meshes and textures setup successfully\n");
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
        if (!renderer) return;

        // Reset object index for consistent material assignment
        renderer->ResetObjectIndex();

        UploadMeshData(renderer);

        UpdateLightConstants(renderer);

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

    void UpdateLightConstants(DX12Renderer* renderer) {
        if (!renderer) return;
        
        // Point light position above and to the side of the scene
        DirectX::XMFLOAT3 lightPosition = { 5.0f, 8.0f, -3.0f };
        DirectX::XMFLOAT3 lightColor = { 1.0f, 0.95f, 0.8f };
        float lightIntensity = 10.0f; // Increased intensity for point light

        static int frameCount = 0;
        if (frameCount % 60 == 0) { // Debug every 60 frames
            Platform::OutputDebugMessage("Light position: (" + 
                std::to_string(lightPosition.x) + ", " + 
                std::to_string(lightPosition.y) + ", " + 
                std::to_string(lightPosition.z) + ")\n");
        }
        frameCount++;

        // Update light constants through renderer
        renderer->UpdateLightConstants(lightPosition, lightColor, lightIntensity);
    }

    void UploadTextureData(DX12Renderer* renderer) {
        Platform::OutputDebugMessage("GameScene: Uploading texture data for all entities...\n");
        
        for (auto& entity : GetEntities()) {
            auto meshComp = entity->GetComponent<MeshComponent>();
            if (meshComp && meshComp->GetMaterial()) {
                auto material = meshComp->GetMaterial();
                auto texture = material->GetTexture("DiffuseTexture");
                if (texture && texture->NeedsUpload()) {
                    Platform::OutputDebugMessage("Uploading texture for entity: " + entity->GetName() + "\n");
                    texture->ForceUpload();
                }
            }
        }
        
        Platform::OutputDebugMessage("GameScene: All texture uploads completed\n");
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

        // Camera settings
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

        // �������� DX12 renderer
        DX12Renderer* dx12Renderer = static_cast<DX12Renderer*>(GetRenderer());
        if (!dx12Renderer) {
            Platform::OutputDebugMessage("RTSApplication: Failed to get DX12 renderer\n");
            return false;
        }

        m_gameScene = std::make_unique<GameScene>();

        m_gameScene->Initialize();

        // Initialize rendering pipeline with integrated shader loading
        if (!dx12Renderer->InitializeRenderingPipeline()) {
            Platform::OutputDebugMessage("RTSApplication: Failed to initialize rendering pipeline\n");
            return false;
        }

        m_gameScene->BeginPlay();

        if (!m_gameScene->SetupMeshes(dx12Renderer)) {
            Platform::OutputDebugMessage("RTSApplication: Failed to setup scene meshes\n");
            return false;
        }

        Platform::OutputDebugMessage("RTSApplication: Initialized successfully!\n");
        Platform::OutputDebugMessage("Textured cubes loaded automatically on startup!\n");
        Platform::OutputDebugMessage("Controls:\n");
        Platform::OutputDebugMessage("  WASD - Move camera\n");
        Platform::OutputDebugMessage("  Right mouse + drag - Look around\n");
        Platform::OutputDebugMessage("  Mouse wheel - Zoom\n");
        Platform::OutputDebugMessage("  R - Reset camera\n");
        Platform::OutputDebugMessage("  F1 - Show entity count\n");
        Platform::OutputDebugMessage("  F2 - Spawn new cube\n");
        Platform::OutputDebugMessage("  F3 - Spawn colored cube\n");
        Platform::OutputDebugMessage("  F4 - Spawn additional cube with bricks.dds texture\n");
        Platform::OutputDebugMessage("  F5 - Spawn additional cube with bricks2.dds texture\n");
        Platform::OutputDebugMessage("  T - Toggle wireframe mode\n");
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

        if (GetCamera()) {
            DirectX::XMMATRIX viewMatrix = GetCamera()->GetViewMatrix();
            DirectX::XMMATRIX projMatrix = GetCamera()->GetProjectionMatrix();
            DirectX::XMFLOAT3 cameraPosition = GetCamera()->GetPosition();

            dx12Renderer->UpdateViewConstants(viewMatrix, projMatrix, cameraPosition);
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

                case KeyCode::F3:
                    if (m_gameScene) {
                        auto newEntity = m_gameScene->SpawnEntity<Entity>();
                        newEntity->SetName("Colored Cube " + std::to_string(m_gameScene->GetEntityCount()));

                        auto transform = newEntity->GetComponent<TransformComponent>();
                        float x = static_cast<float>((rand() % 10) - 5);
                        float z = static_cast<float>((rand() % 10) - 5);
                        transform->SetPosition(x, 2.0f, z);
                        transform->SetScale(0.7f);

                        auto meshComp = newEntity->AddComponent<MeshComponent>();
                        DX12Renderer* renderer = static_cast<DX12Renderer*>(GetRenderer());
                        meshComp->CreateCube(renderer);

                        // Create a texture with random color for demonstration
                        float r = static_cast<float>(rand()) / RAND_MAX;
                        float g = static_cast<float>(rand()) / RAND_MAX;
                        float b = static_cast<float>(rand()) / RAND_MAX;
                        DirectX::XMFLOAT4 randomColor = {r, g, b, 1.0f};
                        
                        Platform::OutputDebugMessage("Created colored cube with color (" + 
                                                    std::to_string(r) + ", " + 
                                                    std::to_string(g) + ", " + 
                                                    std::to_string(b) + ")\n");
                    }
                    break;

                case KeyCode::F4:
                    if (m_gameScene) {
                        auto newEntity = m_gameScene->SpawnEntity<Entity>();
                        newEntity->SetName("Textured Cube (bricks.dds)");

                        auto transform = newEntity->GetComponent<TransformComponent>();
                        float x = static_cast<float>((rand() % 10) - 5);
                        float z = static_cast<float>((rand() % 10) - 5);
                        transform->SetPosition(x, 3.0f, z);
                        transform->SetScale(0.8f);

                        auto meshComp = newEntity->AddComponent<MeshComponent>();
                        DX12Renderer* renderer = static_cast<DX12Renderer*>(GetRenderer());
                        meshComp->CreateCube(renderer);

                        // Load and apply texture
                        meshComp->SetTexture("Assets/Textures/bricks.dds", renderer);

                        Platform::OutputDebugMessage("Created cube with bricks.dds texture\n");
                    }
                    break;

                case KeyCode::F5:
                    if (m_gameScene) {
                        auto newEntity = m_gameScene->SpawnEntity<Entity>();
                        newEntity->SetName("Textured Cube (bricks2.dds)");

                        auto transform = newEntity->GetComponent<TransformComponent>();
                        float x = static_cast<float>((rand() % 10) - 5);
                        float z = static_cast<float>((rand() % 10) - 5);
                        transform->SetPosition(x, 3.5f, z);
                        transform->SetScale(0.9f);

                        auto meshComp = newEntity->AddComponent<MeshComponent>();
                        DX12Renderer* renderer = static_cast<DX12Renderer*>(GetRenderer());
                        meshComp->CreateCube(renderer);

                        // Load and apply texture
                        meshComp->SetTexture("Assets/Textures/bricks2.dds", renderer);

                        Platform::OutputDebugMessage("Created cube with bricks2.dds texture\n");
                    }
                    break;

                case KeyCode::T:
                    {
                        DX12Renderer* dx12Renderer = static_cast<DX12Renderer*>(GetRenderer());
                        if (dx12Renderer) {
                            bool currentMode = dx12Renderer->IsWireframeMode();
                            dx12Renderer->SetWireframeMode(!currentMode);
                            Platform::OutputDebugMessage("Toggled wireframe mode to: " + 
                                                        String(!currentMode ? "ON" : "OFF") + "\n");
                        }
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