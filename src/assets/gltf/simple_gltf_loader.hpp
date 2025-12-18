#pragma once

#include "core/types.hpp"
#include "renderer/backend/gl_mesh.hpp"
#include <string>
#include <vector>

namespace cscpp::assets {

struct SimpleModel {
    renderer::GLMesh mesh;
    bool loaded = false;
};

class SimpleGLTFLoader {
public:
    /// Load a glTF file (simplified)
    Result<SimpleModel> load(const std::string& path);
    
    /// Create a test weapon mesh (for testing without actual glTF file)
    SimpleModel createTestWeaponMesh();
};

} // namespace cscpp::assets

