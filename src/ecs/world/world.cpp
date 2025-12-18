#include "ecs/world/world.hpp"
#include "ecs/systems/system.hpp"
#include "core/logging/logger.hpp"

namespace cscpp::ecs {

World::~World() {
    clear();
}

entt::entity World::createEntity() {
    return m_registry.create();
}

void World::destroyEntity(entt::entity entity) {
    if (m_registry.valid(entity)) {
        m_registry.destroy(entity);
    }
}

bool World::isValid(entt::entity entity) const {
    return m_registry.valid(entity);
}

size_t World::getEntityCount() const {
    // Get entity count from storage
    if (auto* storage = m_registry.storage<entt::entity>(); storage) {
        return storage->size();
    }
    return 0;
}

void World::updatePhase(SystemPhase phase, f32 deltaTime) {
    size_t phaseIndex = static_cast<size_t>(phase);
    
    for (auto& system : m_systems[phaseIndex]) {
        if (system->isEnabled()) {
            system->update(m_registry, deltaTime);
        }
    }
}

void World::update(f32 deltaTime) {
    m_time += deltaTime;
    
    // Update all phases in order
    updatePhase(SystemPhase::PrePhysics, deltaTime);
    updatePhase(SystemPhase::Physics, deltaTime);
    updatePhase(SystemPhase::PostPhysics, deltaTime);
    updatePhase(SystemPhase::PreRender, deltaTime);
    updatePhase(SystemPhase::Render, deltaTime);
    updatePhase(SystemPhase::PostRender, deltaTime);
    updatePhase(SystemPhase::Network, deltaTime);
}

void World::fixedUpdate(f32 fixedDeltaTime) {
    // Fixed update runs physics systems at a fixed rate
    for (size_t i = 0; i < PHASE_COUNT; ++i) {
        for (auto& system : m_systems[i]) {
            if (system->isEnabled()) {
                system->fixedUpdate(m_registry, fixedDeltaTime);
            }
        }
    }
    
    ++m_currentTick;
}

void World::clear() {
    // Shutdown all systems
    for (size_t i = 0; i < PHASE_COUNT; ++i) {
        for (auto& system : m_systems[i]) {
            system->shutdown(m_registry);
        }
        m_systems[i].clear();
    }
    
    // Clear all entities
    m_registry.clear();
    
    m_currentTick = 0;
    m_time = 0.0f;
}

} // namespace cscpp::ecs

