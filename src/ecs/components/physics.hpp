#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"
#include "movement/pm_shared/pm_defs.hpp"
#include <entt/entt.hpp>

namespace cscpp::ecs {

/**
 * @brief Velocity component
 */
struct VelocityComponent {
    Vec3 linear{0.0f};
    Vec3 angular{0.0f};
};

/**
 * @brief Movement state component
 * 
 * Contains all the movement-related state from PlayerMove.
 * This is the authoritative movement state used by the server.
 */
struct MovementComponent {
    Vec3 baseVelocity{0.0f};     ///< Push velocity (conveyor belts)
    Vec3 viewAngles{0.0f};       ///< View angles (pitch, yaw, roll)
    Vec3 punchAngle{0.0f};       ///< Weapon punch/recoil
    
    i32 flags = 0;               ///< FL_ONGROUND, FL_DUCKING, etc.
    i32 waterLevel = 0;          ///< 0-3 water immersion
    i32 useHull = 0;             ///< Current hull (standing/ducked)
    
    f32 duckTime = 0.0f;         ///< Duck transition time
    bool inDuck = false;         ///< In ducking transition
    f32 fallVelocity = 0.0f;     ///< Velocity when started falling
    
    bool onLadder = false;       ///< On a ladder
    Vec3 ladderNormal{0.0f};     ///< Ladder surface normal
    
    f32 maxSpeed = 320.0f;       ///< Current max speed
    
    /// Check if on ground
    bool isOnGround() const { return (flags & movement::FL_ONGROUND) != 0; }
    
    /// Check if ducking
    bool isDucking() const { return (flags & movement::FL_DUCKING) != 0; }
};

/**
 * @brief Collider component
 */
struct ColliderComponent {
    enum class Type {
        Box,
        Capsule,
        Hull,
        Mesh
    };
    
    Type type = Type::Hull;
    Vec3 halfExtents{16.0f, 16.0f, 36.0f};  // Default standing hull
    u32 collisionMask = 0xFFFFFFFF;
    u32 collisionLayer = 1;
};

/**
 * @brief Hitbox component (for hit detection)
 */
struct HitboxComponent {
    struct Hitbox {
        Vec3 mins;
        Vec3 maxs;
        i32 group;  // Head, chest, stomach, etc.
        f32 damageMultiplier;
    };
    
    std::vector<Hitbox> hitboxes;
};

/**
 * @brief Ground entity reference
 */
struct GroundComponent {
    entt::entity groundEntity = entt::null;
    Vec3 groundNormal{0.0f, 1.0f, 0.0f};
};

} // namespace cscpp::ecs

