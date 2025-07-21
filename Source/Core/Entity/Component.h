#pragma once

#include "../Utilities/Types.h"

// Forward declarations
class Entity;
class DX12Renderer;

class Component {
public:
    Component() = default;
    virtual ~Component() = default;

    // Component lifecycle
    virtual void Initialize() {}
    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void Update(float deltaTime) {}
    virtual void Render(DX12Renderer* renderer) {}

    // Owner management
    Entity* GetOwner() const { return m_owner; }
    void SetOwner(Entity* owner) { m_owner = owner; }

    // Component properties
    bool IsActive() const { return m_isActive; }
    void SetActive(bool active) { m_isActive = active; }

protected:
    Entity* m_owner = nullptr;
    bool m_isActive = true;

    DECLARE_NON_COPYABLE(Component);
};