#pragma once

#include "../Utilities/Types.h"
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <typeindex>
#include <unordered_map>

// Forward declarations
class Component;
class Scene;

class Entity {
public:
    Entity(EntityID id = 0);
    virtual ~Entity();

    // Component management
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    template<typename T>
    T* GetComponent() const;

    template<typename T>
    bool HasComponent() const;

    template<typename T>
    bool RemoveComponent();

    // Entity properties
    EntityID GetID() const { return m_id; }
    const String& GetName() const { return m_name; }
    void SetName(const String& name) { m_name = name; }

    bool IsActive() const { return m_isActive; }
    void SetActive(bool active) { m_isActive = active; }

    // Scene association
    Scene* GetScene() const { return m_scene; }
    void SetScene(Scene* scene) { m_scene = scene; }

    // Lifecycle
	virtual void Initialize() {}
    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void Update(float deltaTime);
    virtual void Render(class DX12Renderer* renderer);

protected:
    void RegisterComponent(std::type_index type, UniquePtr<Component> component);
    Component* GetComponentByType(std::type_index type) const;

private:
    EntityID m_id;
    String m_name;
    bool m_isActive = true;
    Scene* m_scene = nullptr;

    // Component storage
    std::unordered_map<std::type_index, UniquePtr<Component>> m_components;

    DECLARE_NON_COPYABLE(Entity);
};

// Template implementations
template<typename T, typename... Args>
T* Entity::AddComponent(Args&&... args) {
    static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

    std::type_index typeIndex(typeid(T));

    // Check if component already exists
    if (m_components.find(typeIndex) != m_components.end()) {
        return static_cast<T*>(m_components[typeIndex].get());
    }

    // Create new component
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T* componentPtr = component.get();

    // Set owner
    component->SetOwner(this);

    // Store component
    RegisterComponent(typeIndex, std::move(component));

    return componentPtr;
}

template<typename T>
T* Entity::GetComponent() const {
    static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

    Component* component = GetComponentByType(std::type_index(typeid(T)));
    return static_cast<T*>(component);
}

template<typename T>
bool Entity::HasComponent() const {
    static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

    return GetComponentByType(std::type_index(typeid(T))) != nullptr;
}

template<typename T>
bool Entity::RemoveComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

    std::type_index typeIndex(typeid(T));
    auto it = m_components.find(typeIndex);

    if (it != m_components.end()) {
        m_components.erase(it);
        return true;
    }

    return false;
}