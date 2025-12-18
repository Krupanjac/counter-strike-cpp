#pragma once

#include "core/types.hpp"
#include <entt/entt.hpp>
#include <vector>
#include <memory>

namespace cscpp::ecs {

// Forward declarations
class System;

/**
 * @brief System execution phases
 */
enum class SystemPhase {
    PrePhysics,     ///< Before physics (input processing)
    Physics,        ///< Physics and movement
    PostPhysics,    ///< After physics (weapons, damage)
    PreRender,      ///< Before rendering (animation, culling)
    Render,         ///< Rendering
    PostRender,     ///< After rendering (debug, cleanup)
    Network         ///< Network send/receive
};

/**
 * @brief ECS World
 * 
 * Manages entities, components, and systems.
 */
class World {
public:
    World() = default;
    ~World();
    
    /// Get the entity registry
    entt::registry& getRegistry() { return m_registry; }
    const entt::registry& getRegistry() const { return m_registry; }
    
    // ========================================================================
    // Entity Management
    // ========================================================================
    
    /// Create a new entity
    entt::entity createEntity();
    
    /// Destroy an entity
    void destroyEntity(entt::entity entity);
    
    /// Check if entity is valid
    bool isValid(entt::entity entity) const;
    
    /// Get entity count
    size_t getEntityCount() const;
    
    // ========================================================================
    // Component Helpers
    // ========================================================================
    
    /// Add a component to an entity
    template<typename T, typename... Args>
    T& addComponent(entt::entity entity, Args&&... args) {
        return m_registry.emplace<T>(entity, std::forward<Args>(args)...);
    }
    
    /// Get a component from an entity
    template<typename T>
    T& getComponent(entt::entity entity) {
        return m_registry.get<T>(entity);
    }
    
    /// Get a component (const)
    template<typename T>
    const T& getComponent(entt::entity entity) const {
        return m_registry.get<T>(entity);
    }
    
    /// Try to get a component (returns nullptr if not found)
    template<typename T>
    T* tryGetComponent(entt::entity entity) {
        return m_registry.try_get<T>(entity);
    }
    
    /// Check if entity has a component
    template<typename T>
    bool hasComponent(entt::entity entity) const {
        return m_registry.all_of<T>(entity);
    }
    
    /// Remove a component from an entity
    template<typename T>
    void removeComponent(entt::entity entity) {
        m_registry.remove<T>(entity);
    }
    
    /// Get a view of entities with specific components
    template<typename... Components>
    auto view() {
        return m_registry.view<Components...>();
    }
    
    /// Get a group of entities with specific components
    template<typename... Owned, typename... Get, typename... Exclude>
    auto group(entt::get_t<Get...> = {}, entt::exclude_t<Exclude...> = {}) {
        return m_registry.group<Owned...>(entt::get<Get...>, entt::exclude<Exclude...>);
    }
    
    // ========================================================================
    // System Management
    // ========================================================================
    
    /// Register a system
    template<typename T, typename... Args>
    T* registerSystem(SystemPhase phase, Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        m_systems[static_cast<size_t>(phase)].push_back(std::move(system));
        return ptr;
    }
    
    /// Update all systems in a phase
    void updatePhase(SystemPhase phase, f32 deltaTime);
    
    /// Update all systems in order
    void update(f32 deltaTime);
    
    /// Fixed update (for physics tick)
    void fixedUpdate(f32 fixedDeltaTime);
    
    // ========================================================================
    // World State
    // ========================================================================
    
    /// Clear all entities and reset world
    void clear();
    
    /// Get current tick
    Tick getCurrentTick() const { return m_currentTick; }
    
    /// Set current tick
    void setCurrentTick(Tick tick) { m_currentTick = tick; }
    
    /// Increment tick
    void incrementTick() { ++m_currentTick; }
    
    /// Get elapsed time
    f32 getTime() const { return m_time; }
    
private:
    entt::registry m_registry;
    
    // Systems organized by phase
    static constexpr size_t PHASE_COUNT = 7;
    std::vector<std::unique_ptr<System>> m_systems[PHASE_COUNT];
    
    Tick m_currentTick = 0;
    f32 m_time = 0.0f;
};

} // namespace cscpp::ecs

