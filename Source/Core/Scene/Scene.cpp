#include "Scene.h"
#include "../Entity/Entity.h"
#include "../Entity/TransformComponent.h"
#include "../../Platform/Windows/WindowsPlatform.h"

Scene::Scene() {
    Platform::OutputDebugMessage("Scene created\n");
}

Scene::~Scene() {
    EndPlay();
}

bool Scene::DestroyEntity(EntityID id) {
    Entity* entity = FindEntity(id);
    return entity ? DestroyEntity(entity) : false;
}

bool Scene::DestroyEntity(Entity* entity) {
    if (!entity) return false;

    // Find the entity in our vector
    auto it = std::find_if(m_entities.begin(), m_entities.end(),
        [entity](const UniquePtr<Entity>& ptr) { return ptr.get() == entity; });

    if (it != m_entities.end()) {
        // Notify derived class
        OnEntityDestroyed(entity);

        // Call EndPlay on entity
        entity->EndPlay();

        // Unregister from lookup
        UnregisterEntity(entity);

        // Remove from vector
        m_entities.erase(it);

        return true;
    }

    return false;
}

Entity* Scene::FindEntity(EntityID id) const {
    auto it = m_entityLookup.find(id);
    return (it != m_entityLookup.end()) ? it->second : nullptr;
}

Entity* Scene::FindEntityByName(const String& name) const {
    for (const auto& entity : m_entities) {
        if (entity->GetName() == name) {
            return entity.get();
        }
    }
    return nullptr;
}

void Scene::Initialize() {
    Platform::OutputDebugMessage("Scene initializing: " + m_name + "\n");

    // Initialize all existing entities
    for (auto& entity : m_entities) {
        if (entity) {
            entity->Initialize();
        }
    }
}

void Scene::BeginPlay() {
    if (!m_isActive) return;

    Platform::OutputDebugMessage("Scene begin play: " + m_name + "\n");

    // Call BeginPlay on all entities
    for (auto& entity : m_entities) {
        if (entity && entity->IsActive()) {
            entity->BeginPlay();
        }
    }
}

void Scene::EndPlay() {
    Platform::OutputDebugMessage("Scene end play: " + m_name + "\n");

    // Call EndPlay on all entities
    for (auto& entity : m_entities) {
        if (entity) {
            entity->EndPlay();
        }
    }
}

void Scene::Update(float deltaTime) {
    if (!m_isActive) return;

    // Update all entities
    for (auto& entity : m_entities) {
        if (entity && entity->IsActive()) {
            entity->Update(deltaTime);
        }
    }
}

void Scene::Render(DX12Renderer* renderer) {
    if (!m_isActive || !renderer) return;

    // Render all entities
    for (auto& entity : m_entities) {
        if (entity && entity->IsActive()) {
            entity->Render(renderer);
        }
    }
}

EntityID Scene::GenerateEntityID() {
    return m_nextEntityID++;
}

void Scene::RegisterEntity(Entity* entity) {
    if (entity) {
        m_entityLookup[entity->GetID()] = entity;
    }
}

void Scene::UnregisterEntity(Entity* entity) {
    if (entity) {
        auto it = m_entityLookup.find(entity->GetID());
        if (it != m_entityLookup.end()) {
            m_entityLookup.erase(it);
        }
    }
}