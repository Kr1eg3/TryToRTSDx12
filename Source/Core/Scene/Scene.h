#pragma once

#include "../Utilities/Types.h"
#include "../Entity/Entity.h"
#include "../Entity/TransformComponent.h"
#include <vector>
#include <memory>
#include <unordered_map>

class Renderer;
class DX12Renderer;

class Scene {
public:
    Scene();
    virtual ~Scene();

    // Entity management
    template<typename T = Entity, typename... Args>
    T* SpawnEntity(Args&&... args);

    bool DestroyEntity(EntityID id);
    bool DestroyEntity(Entity* entity);

    Entity* FindEntity(EntityID id) const;
    Entity* FindEntityByName(const String& name) const;

    const Vector<UniquePtr<Entity>>& GetEntities() const { return m_entities; }
    size_t GetEntityCount() const { return m_entities.size(); }

    // Scene lifecycle
    virtual void Initialize();
    virtual void BeginPlay();
    virtual void EndPlay();
    virtual void Update(float deltaTime);

    // ���������� ��� �������������
    virtual void Render(Renderer* renderer);
    virtual void Render(DX12Renderer* renderer); // �������� �������������
    virtual void Render(class IRHIContext& context); // ����� RHI �������������

    // Scene properties
    const String& GetName() const { return m_name; }
    void SetName(const String& name) { m_name = name; }

    bool IsActive() const { return m_isActive; }
    void SetActive(bool active) { m_isActive = active; }


protected:
    virtual void OnEntitySpawned(Entity* entity) {}
    virtual void OnEntityDestroyed(Entity* entity) {}

private:
    String m_name = "Untitled Scene";
    bool m_isActive = true;

    Vector<UniquePtr<Entity>> m_entities;
    std::unordered_map<EntityID, Entity*> m_entityLookup;

    EntityID m_nextEntityID = 1;

    // Internal helpers
    EntityID GenerateEntityID();
    void RegisterEntity(Entity* entity);
    void UnregisterEntity(Entity* entity);

    DECLARE_NON_COPYABLE(Scene);
};

// Template implementation
template<typename T, typename... Args>
T* Scene::SpawnEntity(Args&&... args) {
    static_assert(std::is_base_of_v<Entity, T>, "T must derive from Entity");

    EntityID id = GenerateEntityID();
    auto entity = std::make_unique<T>(id, std::forward<Args>(args)...);
    T* entityPtr = entity.get();

    // Set scene reference
    entityPtr->SetScene(this);

    // Add transform component by default
    entityPtr->template AddComponent<TransformComponent>();

    // Store entity
    RegisterEntity(entityPtr);
    m_entities.push_back(std::move(entity));

    // Initialize entity
    entityPtr->Initialize();

    // Notify derived class
    OnEntitySpawned(entityPtr);

    return entityPtr;
}