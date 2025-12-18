#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"
#include "renderer/backend/gl_mesh.hpp"
#include "assets/bsp/bsp_format.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>

namespace cscpp::assets {

// Mesh group for a single texture
struct BSPMeshGroup {
    std::vector<renderer::Vertex> vertices;
    std::vector<u32> indices;
    renderer::GLMesh mesh;
    u32 textureID;  // OpenGL texture ID for this group
    i32 miptexIndex;  // BSP miptex index (for reference)
};

struct SimpleBSPMesh {
    std::vector<BSPMeshGroup> groups;  // One mesh group per texture
    std::unordered_map<i32, u32> textureMap;  // Maps miptex index to OpenGL texture ID
    AABB bounds;  // Map bounding box for collision/bounds checking
    bool loaded = false;
};

class SimpleBSPLoader {
public:
    /// Load a BSP file
    Result<SimpleBSPMesh> load(const std::string& path);
    
    /// Create a test mesh (for testing without actual BSP file)
    SimpleBSPMesh createTestMesh();
    
private:
    /// Parse BSP file
    Result<SimpleBSPMesh> parseBSP(std::ifstream& file, const std::string& path);
    
    /// Read BSP lump data
    bool readLump(std::ifstream& file, bsp::BSPLumpType lumpType, 
                  bsp::BSPHeader& header, std::vector<u8>& data);
    
    /// Load textures from BSP texture lump
    bool loadTextures(std::ifstream& file, const bsp::BSPHeader& header, 
                      SimpleBSPMesh& mesh, const bsp::BSPTextureInfo* texInfos, u32 texInfoCount,
                      const std::string& path);
    
    /// Load textures from WAD files
    bool loadWADTextures(const std::string& bspPath, SimpleBSPMesh& mesh, 
                         const std::vector<std::string>& textureNames,
                         const std::unordered_map<std::string, i32>& textureNameToIndex);
    
    /// Load a single WAD file
    bool loadWADFile(const std::string& wadPath, SimpleBSPMesh& mesh, 
                     const std::vector<std::string>& neededTextures,
                     const std::unordered_map<std::string, i32>& textureNameToIndex);
    
    /// Create OpenGL texture from miptex data
    u32 createTextureFromMiptex(const bsp::BSPMiptex& miptex, const u8* data, u32 dataSize);
};

} // namespace cscpp::assets

