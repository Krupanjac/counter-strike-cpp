#pragma once

#include "core/types.hpp"
#include <entt/entt.hpp>

namespace cscpp::ecs {

/**
 * @brief Base class for ECS systems
 */
class System {
public:
    virtual ~System() = default;
    
    /// Called each frame update
    virtual void update(entt::registry& registry, f32 deltaTime) {
        (void)registry;
        (void)deltaTime;
    }
    
    /// Called each fixed physics tick
    virtual void fixedUpdate(entt::registry& registry, f32 fixedDeltaTime) {
        (void)registry;
        (void)fixedDeltaTime;
    }
    
    /// Called when system is registered
    virtual void initialize(entt::registry& registry) {
        (void)registry;
    }
    
    /// Called when system is unregistered
    virtual void shutdown(entt::registry& registry) {
        (void)registry;
    }
    
    /// Enable/disable the system
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
protected:
    bool m_enabled = true;
};

} // namespace cscpp::ecs

