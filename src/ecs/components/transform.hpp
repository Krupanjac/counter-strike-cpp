#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"
#include <entt/entt.hpp>

namespace cscpp::ecs {

/**
 * @brief Transform component
 * 
 * Position, rotation, and scale in world space.
 */
struct TransformComponent {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion
    Vec3 scale{1.0f};
    
    /// Get the world transformation matrix
    Mat4 getMatrix() const {
        return math::compose(position, rotation, scale);
    }
    
    /// Get forward direction
    Vec3 getForward() const {
        return rotation * Vec3(0.0f, 0.0f, -1.0f);
    }
    
    /// Get right direction
    Vec3 getRight() const {
        return rotation * Vec3(1.0f, 0.0f, 0.0f);
    }
    
    /// Get up direction
    Vec3 getUp() const {
        return rotation * Vec3(0.0f, 1.0f, 0.0f);
    }
    
    /// Set rotation from Euler angles (degrees)
    void setRotationEuler(Vec3 euler) {
        rotation = glm::quat(glm::radians(euler));
    }
    
    /// Get Euler angles (degrees)
    Vec3 getRotationEuler() const {
        return glm::degrees(glm::eulerAngles(rotation));
    }
};

/**
 * @brief Local transform component (relative to parent)
 * 
 * Used for hierarchical transforms.
 */
struct LocalTransformComponent {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};
    
    Mat4 getMatrix() const {
        return math::compose(position, rotation, scale);
    }
};

/**
 * @brief Parent relationship component
 */
struct ParentComponent {
    entt::entity parent = entt::null;
};

/**
 * @brief Children relationship component
 */
struct ChildrenComponent {
    std::vector<entt::entity> children;
};

/**
 * @brief Previous frame transform (for interpolation)
 */
struct PreviousTransformComponent {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace cscpp::ecs

