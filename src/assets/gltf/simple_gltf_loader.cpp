#include "assets/gltf/simple_gltf_loader.hpp"
#include "core/logging/logger.hpp"
#include <fstream>

namespace cscpp::assets {

Result<SimpleModel> SimpleGLTFLoader::load(const std::string& path) {
    LOG_INFO("Loading glTF model: {}", path);
    
    // Try multiple paths
    std::vector<std::string> tryPaths = {
        path,
        "../" + path,
        "../../" + path,
        "assets/weapons/ak-47.gltf",
        "../assets/weapons/ak-47.gltf",
        "../../assets/weapons/ak-47.gltf",
    };
    
    std::ifstream file;
    std::string foundPath;
    for (const auto& tryPath : tryPaths) {
        file.open(tryPath);
        if (file.is_open()) {
            foundPath = tryPath;
            break;
        }
    }
    
    if (!file.is_open()) {
        LOG_WARN("glTF file not found in any location, creating test mesh");
        return createTestWeaponMesh();
    }
    
    file.close();
    LOG_INFO("Found glTF file at: {}", foundPath);
    
    // TODO: Implement actual glTF loading with tinygltf
    // For now, return test mesh
    LOG_WARN("glTF parsing not yet implemented, using test mesh");
    return createTestWeaponMesh();
}

SimpleModel SimpleGLTFLoader::createTestWeaponMesh() {
    SimpleModel model;
    
    std::vector<renderer::Vertex> vertices;
    std::vector<u32> indices;
    
    // Create a simple box to represent the weapon
    // This is a placeholder - will be replaced with actual model loading
    
    // Front face
    vertices.push_back({{-0.1f, -0.05f,  0.3f}, {0, 0, 1}, {0, 0}});
    vertices.push_back({{ 0.1f, -0.05f,  0.3f}, {0, 0, 1}, {1, 0}});
    vertices.push_back({{ 0.1f,  0.05f,  0.3f}, {0, 0, 1}, {1, 1}});
    vertices.push_back({{-0.1f,  0.05f,  0.3f}, {0, 0, 1}, {0, 1}});
    
    u32 base = 0;
    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    
    // Back face
    vertices.push_back({{-0.1f, -0.05f, -0.1f}, {0, 0, -1}, {0, 0}});
    vertices.push_back({{ 0.1f, -0.05f, -0.1f}, {0, 0, -1}, {1, 0}});
    vertices.push_back({{ 0.1f,  0.05f, -0.1f}, {0, 0, -1}, {1, 1}});
    vertices.push_back({{-0.1f,  0.05f, -0.1f}, {0, 0, -1}, {0, 1}});
    
    base = 4;
    indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
    indices.push_back(base + 0); indices.push_back(base + 3); indices.push_back(base + 2);
    
    // Top, bottom, left, right faces...
    // (simplified - just add a few more faces)
    
    // Top
    vertices.push_back({{-0.1f, 0.05f, -0.1f}, {0, 1, 0}, {0, 0}});
    vertices.push_back({{ 0.1f, 0.05f, -0.1f}, {0, 1, 0}, {1, 0}});
    vertices.push_back({{ 0.1f, 0.05f,  0.3f}, {0, 1, 0}, {1, 1}});
    vertices.push_back({{-0.1f, 0.05f,  0.3f}, {0, 1, 0}, {0, 1}});
    
    base = 8;
    indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
    
    // Bottom
    vertices.push_back({{-0.1f, -0.05f, -0.1f}, {0, -1, 0}, {0, 0}});
    vertices.push_back({{ 0.1f, -0.05f, -0.1f}, {0, -1, 0}, {1, 0}});
    vertices.push_back({{ 0.1f, -0.05f,  0.3f}, {0, -1, 0}, {1, 1}});
    vertices.push_back({{-0.1f, -0.05f,  0.3f}, {0, -1, 0}, {0, 1}});
    
    base = 12;
    indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
    indices.push_back(base + 0); indices.push_back(base + 3); indices.push_back(base + 2);
    
    model.mesh.create(vertices, indices);
    model.loaded = true;
    
    LOG_INFO("Created test weapon mesh with {} vertices, {} indices", vertices.size(), indices.size());
    
    return model;
}

} // namespace cscpp::assets

