#pragma once

/**
 * @file assets.hpp
 * @brief Asset loading and management
 */

#include "core/types.hpp"
#include "core/math/math.hpp"
#include <string>
#include <vector>

namespace cscpp::assets {

// ============================================================================
// Asset Types
// ============================================================================

/**
 * @brief Mesh vertex data
 */
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec4 tangent;  // w = handedness
    Vec2 texCoord;
    Vec4 joints;   // For skinning
    Vec4 weights;  // For skinning
};

/**
 * @brief Mesh asset
 */
struct MeshAsset {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    AABB boundingBox;
    
    struct SubMesh {
        u32 indexOffset;
        u32 indexCount;
        u32 materialIndex;
    };
    std::vector<SubMesh> subMeshes;
    
    // LOD levels
    struct LOD {
        u32 indexOffset;
        u32 indexCount;
        f32 screenSizeThreshold;
    };
    std::vector<LOD> lods;
};

/**
 * @brief PBR Material asset
 */
struct MaterialAsset {
    std::string name;
    
    // Textures (paths or handles)
    std::string albedoMap;
    std::string normalMap;
    std::string metallicRoughnessMap;
    std::string aoMap;
    std::string emissiveMap;
    
    // Factors
    Vec4 baseColorFactor{1.0f};
    f32 metallicFactor = 1.0f;
    f32 roughnessFactor = 1.0f;
    Vec3 emissiveFactor{0.0f};
    
    // Alpha
    enum class AlphaMode { Opaque, Mask, Blend };
    AlphaMode alphaMode = AlphaMode::Opaque;
    f32 alphaCutoff = 0.5f;
    
    bool doubleSided = false;
};

/**
 * @brief Skeleton joint
 */
struct Joint {
    std::string name;
    i32 parentIndex = -1;
    Mat4 inverseBindMatrix;
    Mat4 localTransform;
};

/**
 * @brief Skeleton asset
 */
struct SkeletonAsset {
    std::vector<Joint> joints;
    std::vector<std::string> jointNames;  // For lookup
};

/**
 * @brief Animation keyframe
 */
template<typename T>
struct Keyframe {
    f32 time;
    T value;
};

/**
 * @brief Animation channel
 */
struct AnimationChannel {
    i32 targetJoint;
    
    enum class Path { Translation, Rotation, Scale };
    Path path;
    
    enum class Interpolation { Step, Linear, CubicSpline };
    Interpolation interpolation;
    
    std::vector<Keyframe<Vec3>> vec3Keys;
    std::vector<Keyframe<Quat>> quatKeys;
};

/**
 * @brief Animation clip asset
 */
struct AnimationAsset {
    std::string name;
    f32 duration;
    std::vector<AnimationChannel> channels;
};

/**
 * @brief Complete model asset (mesh + materials + skeleton + animations)
 */
struct ModelAsset {
    std::vector<MeshAsset> meshes;
    std::vector<MaterialAsset> materials;
    SkeletonAsset skeleton;
    std::vector<AnimationAsset> animations;
};

// ============================================================================
// Asset Loaders
// ============================================================================

/**
 * @brief Load a glTF model
 */
Result<ModelAsset> loadGLTF(const std::string& path);

/**
 * @brief Load a BSP map
 */
struct BSPAsset {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    // Collision data
    // Visibility data (PVS)
    // Entities
};
Result<BSPAsset> loadBSP(const std::string& path);

/**
 * @brief Load a texture
 */
struct TextureAsset {
    u32 width;
    u32 height;
    u32 format;
    u32 mipLevels;
    std::vector<u8> data;
};
Result<TextureAsset> loadTexture(const std::string& path);

/**
 * @brief Load a KTX2 compressed texture
 */
Result<TextureAsset> loadKTX2(const std::string& path);

// ============================================================================
// Asset Manager
// ============================================================================

/**
 * @brief Manages loaded assets with caching
 */
class AssetManager {
public:
    /// Load a model (cached)
    Result<MeshHandle> loadMesh(const std::string& path);
    
    /// Load a texture (cached)
    Result<TextureHandle> loadTexture(const std::string& path);
    
    /// Load a material
    Result<MaterialHandle> loadMaterial(const std::string& path);
    
    /// Unload unused assets
    void collectGarbage();
    
    /// Get memory usage
    struct MemoryStats {
        size_t meshMemory;
        size_t textureMemory;
        size_t totalMemory;
        i32 meshCount;
        i32 textureCount;
    };
    MemoryStats getMemoryStats() const;
    
private:
    // Asset caches would be here
};

} // namespace cscpp::assets

