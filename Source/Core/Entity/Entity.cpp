#include "Entity.h"
#include "Component.h"
#include "../../Platform/Windows/WindowsPlatform.h"

Entity::Entity(EntityID id) : m_id(id) {
    if (id == 0) {
        // Generate a simple ID if none provided (for standalone entities)
        static EntityID nextID = 1;
        m_id = nextID++;
    }
}

Entity::~Entity() {
    // Components are automatically destroyed by unique_ptr
}

void Entity::Update(float deltaTime) {
    if (!m_isActive) return;

    for (auto& [type, component] : m_components) {
        if (component && component->IsActive()) {
            component->Update(deltaTime);
        }
    }
}

void Entity::Render(DX12Renderer* renderer) {
    if (!m_isActive || !renderer) return;

    // Render all components
    for (auto& [type, component] : m_components) {
        if (component && component->IsActive()) {
            component->Render(renderer);
        }
    }
}

void Entity::RegisterComponent(std::type_index type, UniquePtr<Component> component) {
    m_components[type] = std::move(component);
}

Component* Entity::GetComponentByType(std::type_index type) const {
    auto it = m_components.find(type);
    return (it != m_components.end()) ? it->second.get() : nullptr;
}