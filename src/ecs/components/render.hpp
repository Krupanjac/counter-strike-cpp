#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"

namespace cscpp::ecs {

/**
 * @brief Renderable mesh component
 */
struct RenderableComponent {
    MeshHandle mesh;
    MaterialHandle material;
    
    u32 renderFlags = 0;
    
    enum Flags : u32 {
        Visible      = 1 << 0,
        CastShadow   = 1 << 1,
        ReceiveShadow = 1 << 2,
        Transparent  = 1 << 3,
    };
    
    bool isVisible() const { return renderFlags & Visible; }
    bool castsShadow() const { return renderFlags & CastShadow; }
    bool receivesShadow() const { return renderFlags & ReceiveShadow; }
    bool isTransparent() const { return renderFlags & Transparent; }
    
    RenderableComponent() : renderFlags(Visible | CastShadow | ReceiveShadow) {}
};

/**
 * @brief Bounding volumes for culling
 */
struct BoundsComponent {
    AABB localBounds;     // Local space AABB
    AABB worldBounds;     // World space AABB (updated each frame)
    f32 boundingSphereRadius = 0.0f;
};

/**
 * @brief LOD (Level of Detail) component
 */
struct LODComponent {
    struct Level {
        MeshHandle mesh;
        f32 minDistance;
        f32 maxDistance;
    };
    
    std::vector<Level> levels;
    i32 currentLOD = 0;
    f32 lodBias = 1.0f;  // Multiplier for LOD distances
};

/**
 * @brief Skeletal animation component
 */
struct AnimationComponent {
    struct AnimState {
        u32 animationId = 0;
        f32 time = 0.0f;
        f32 speed = 1.0f;
        f32 weight = 1.0f;
        bool looping = true;
    };
    
    AnimState currentAnim;
    AnimState blendAnim;      // For animation blending
    f32 blendFactor = 0.0f;   // 0 = current, 1 = blend
    f32 blendDuration = 0.0f;
    
    std::vector<Mat4> jointMatrices;  // Skinning matrices
};

/**
 * @brief Camera component
 */
struct CameraComponent {
    f32 fov = 90.0f;          // Field of view in degrees
    f32 nearPlane = 0.1f;
    f32 farPlane = 10000.0f;
    f32 aspectRatio = 16.0f / 9.0f;
    
    Vec3 viewOffset{0.0f, 0.0f, 64.0f};  // Eye offset from entity origin
    
    Mat4 getProjectionMatrix() const {
        return math::perspectiveReversedZ(
            math::radians(fov),
            aspectRatio,
            nearPlane
        );
    }
    
    Mat4 getViewMatrix(const Vec3& position, const Quat& rotation) const {
        Vec3 eye = position + viewOffset;
        Vec3 forward = rotation * Vec3(0.0f, 0.0f, -1.0f);
        Vec3 up = rotation * Vec3(0.0f, 1.0f, 0.0f);
        return math::lookAt(eye, eye + forward, up);
    }
};

/**
 * @brief Point light component
 */
struct PointLightComponent {
    Vec3 color{1.0f};
    f32 intensity = 1.0f;
    f32 radius = 10.0f;
    bool castsShadows = false;
};

/**
 * @brief Spot light component
 */
struct SpotLightComponent {
    Vec3 color{1.0f};
    f32 intensity = 1.0f;
    f32 range = 10.0f;
    f32 innerAngle = 30.0f;  // Degrees
    f32 outerAngle = 45.0f;
    bool castsShadows = true;
};

/**
 * @brief Directional light component (sun)
 */
struct DirectionalLightComponent {
    Vec3 color{1.0f};
    f32 intensity = 1.0f;
    bool castsShadows = true;
    i32 shadowCascades = 4;
};

/**
 * @brief Particle emitter component
 */
struct ParticleEmitterComponent {
    u32 particleSystemId = 0;
    bool isPlaying = true;
    f32 playbackSpeed = 1.0f;
};

/**
 * @brief Decal component
 */
struct DecalComponent {
    TextureHandle texture;
    Vec3 size{1.0f};
    f32 fadeDistance = 100.0f;
    f32 lifetime = -1.0f;  // -1 = permanent
    f32 age = 0.0f;
};

/**
 * @brief First-person view model component
 */
struct ViewModelComponent {
    MeshHandle mesh;
    MaterialHandle material;
    Vec3 offset{0.0f};      // Offset from camera
    Vec3 bobOffset{0.0f};   // View bob offset
    f32 swayAmount = 0.0f;  // Weapon sway
};

} // namespace cscpp::ecs

