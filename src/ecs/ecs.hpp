#pragma once

/**
 * @file ecs.hpp
 * @brief Entity-Component-System Module
 * 
 * This module provides ECS integration using entt.
 * It defines common components and system interfaces.
 */

#include "ecs/components/transform.hpp"
#include "ecs/components/physics.hpp"
#include "ecs/components/player.hpp"
#include "ecs/components/network.hpp"
#include "ecs/components/render.hpp"
#include "ecs/world/world.hpp"
#include "ecs/systems/system.hpp"

namespace cscpp::ecs {

// Re-export common types
using Entity = entt::entity;
using Registry = entt::registry;

constexpr Entity NullEntity = entt::null;

} // namespace cscpp::ecs

