#pragma once

#include "core/types.hpp"
#include "renderer/backend/gl_mesh.hpp"
#include <string>
#include <vector>

namespace cscpp::assets {

struct SimpleModel {
    renderer::GLMesh mesh;
    u32 textureID = 0;  // OpenGL texture ID (0 = no texture)
    bool loaded = false;
};

class SimpleGLTFLoader {
public:
    /// Load a glTF file (simplified)
    Result<SimpleModel> load(const std::string& path);
    
    /// Create a test weapon mesh (for testing without actual glTF file)
    SimpleModel createTestWeaponMesh();

private:
#if defined(TINYGLTF_FOUND) || __has_include(<tiny_gltf.h>)
    /// Load glTF using tinygltf library
    Result<SimpleModel> loadGLTF(const std::string& path);
#endif
};

} // namespace cscpp::assets

