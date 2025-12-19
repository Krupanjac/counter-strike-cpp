#include "assets/bsp/simple_bsp_loader.hpp"
#include "core/logging/logger.hpp"
#include <glad/glad.h>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <algorithm>

namespace cscpp::assets {

Result<SimpleBSPMesh> SimpleBSPLoader::load(const std::string& path) {
    LOG_INFO("Loading BSP map: {}", path);
    
    // Try multiple paths
    std::vector<std::string> tryPaths = {
        path,
        "../" + path,
        "../../" + path,
        "assets/maps/de_dust2.bsp",
        "../assets/maps/de_dust2.bsp",
        "../../assets/maps/de_dust2.bsp",
    };
    
    std::ifstream file;
    std::string foundPath;
    for (const auto& tryPath : tryPaths) {
        file.open(tryPath, std::ios::binary);
        if (file.is_open()) {
            foundPath = tryPath;
            break;
        }
    }
    
    if (!file.is_open()) {
        LOG_WARN("BSP file not found in any location, creating test mesh");
        return createTestMesh();
    }
    
    LOG_INFO("Found BSP file at: {}", foundPath);
    
    // Try to parse the BSP file
    auto result = parseBSP(file, foundPath);
    file.close();
    
    if (!result) {
        LOG_WARN("Failed to parse BSP file: {}, using test mesh", result.error().message);
        return createTestMesh();
    }
    
    return result;
}

Result<SimpleBSPMesh> SimpleBSPLoader::parseBSP(std::ifstream& file, const std::string& path) {
    SimpleBSPMesh mesh;
    
    // Read BSP header
    bsp::BSPHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!file.good() || file.gcount() != sizeof(header)) {
        return std::unexpected(Error{"Failed to read BSP header"});
    }
    
    // Check BSP version (30 for GoldSrc)
    if (header.version != 30) {
        return std::unexpected(Error{"Unsupported BSP version: " + std::to_string(header.version)});
    }
    
    LOG_INFO("BSP version: {}", header.version);
    
    // Log lump info for debugging
    for (int i = 0; i < 15; ++i) {
        if (header.lumps[i].length > 0) {
            LOG_INFO("Lump {}: offset={}, length={}", i, header.lumps[i].offset, header.lumps[i].length);
        }
    }
    
    // Read vertices
    std::vector<u8> vertexData;
    if (!readLump(file, bsp::LUMP_VERTICES, header, vertexData)) {
        return std::unexpected(Error{"Failed to read vertices lump"});
    }
    
    u32 vertexCount = static_cast<u32>(vertexData.size() / sizeof(bsp::BSPVertex));
    LOG_INFO("BSP vertex count: {}", vertexCount);
    
    if (vertexCount == 0) {
        return std::unexpected(Error{"BSP file has no vertices"});
    }
    
    // Read edges
    std::vector<u8> edgeData;
    if (!readLump(file, bsp::LUMP_EDGES, header, edgeData)) {
        return std::unexpected(Error{"Failed to read edges lump"});
    }
    
    u32 edgeCount = static_cast<u32>(edgeData.size() / sizeof(bsp::BSPEdge));
    LOG_INFO("BSP edge count: {}", edgeCount);
    
    // Read surfedges
    std::vector<u8> surfedgeData;
    if (!readLump(file, bsp::LUMP_SURFEDGES, header, surfedgeData)) {
        return std::unexpected(Error{"Failed to read surfedges lump"});
    }
    
    u32 surfedgeCount = static_cast<u32>(surfedgeData.size() / sizeof(i32));
    LOG_INFO("BSP surfedge count: {}", surfedgeCount);
    
    // Read faces
    std::vector<u8> faceData;
    if (!readLump(file, bsp::LUMP_FACES, header, faceData)) {
        return std::unexpected(Error{"Failed to read faces lump"});
    }
    
    u32 faceCount = static_cast<u32>(faceData.size() / sizeof(bsp::BSPFace));
    LOG_INFO("BSP face count: {}", faceCount);
    
    if (faceCount == 0) {
        return std::unexpected(Error{"BSP file has no faces"});
    }
    
    // Parse vertices
    const bsp::BSPVertex* bspVertices = reinterpret_cast<const bsp::BSPVertex*>(vertexData.data());
    std::vector<Vec3> positions;
    positions.reserve(vertexCount);
    for (u32 i = 0; i < vertexCount; ++i) {
        // Simple swap: GoldSrc (X,Y,Z) -> OpenGL (Z,Y,X)
        // Transformation matrix applied in render function handles final orientation
        positions.push_back(Vec3(
            bspVertices[i].position[2],   // GoldSrc Z -> OpenGL X
            bspVertices[i].position[1],   // GoldSrc Y -> OpenGL Y (up stays up)
            bspVertices[i].position[0]    // GoldSrc X -> OpenGL Z
        ));
    }
    
    // Read planes for normal calculation
    std::vector<u8> planeData;
    if (!readLump(file, bsp::LUMP_PLANES, header, planeData)) {
        return std::unexpected(Error{"Failed to read planes lump"});
    }
    
    u32 planeCount = static_cast<u32>(planeData.size() / sizeof(bsp::BSPPlane));
    LOG_INFO("BSP plane count: {}", planeCount);
    
    const bsp::BSPPlane* planes = reinterpret_cast<const bsp::BSPPlane*>(planeData.data());
    
    // Read texture info for texture coordinates
    std::vector<u8> texInfoData;
    if (!readLump(file, bsp::LUMP_TEXINFO, header, texInfoData)) {
        LOG_WARN("Failed to read texture info lump, textures will not be available");
    }
    
    u32 texInfoCount = 0;
    const bsp::BSPTextureInfo* texInfos = nullptr;
    if (!texInfoData.empty()) {
        texInfoCount = static_cast<u32>(texInfoData.size() / sizeof(bsp::BSPTextureInfo));
        LOG_INFO("BSP texture info count: {}", texInfoCount);
        texInfos = reinterpret_cast<const bsp::BSPTextureInfo*>(texInfoData.data());
    }
    
    // Parse edges
    const bsp::BSPEdge* edges = reinterpret_cast<const bsp::BSPEdge*>(edgeData.data());
    const i32* surfedges = reinterpret_cast<const i32*>(surfedgeData.data());
    const bsp::BSPFace* faces = reinterpret_cast<const bsp::BSPFace*>(faceData.data());
    
    // Group faces by miptex index (texture)
    // Map: miptex index -> vector of face indices
    std::unordered_map<i32, std::vector<u32>> facesByTexture;
    
    // First pass: group faces by texture
    for (u32 faceIdx = 0; faceIdx < faceCount; ++faceIdx) {
        const bsp::BSPFace& face = faces[faceIdx];
        if (face.numEdges < 3) continue;
        
        // Get miptex index from texture info
        i32 miptexIndex = -1;
        if (texInfos != nullptr && face.textureInfo < texInfoCount) {
            miptexIndex = texInfos[face.textureInfo].miptex;
        }
        
        facesByTexture[miptexIndex].push_back(faceIdx);
    }
    
    LOG_INFO("Grouped {} faces into {} texture groups", faceCount, facesByTexture.size());
    
    // Convert BSP faces to triangles, grouped by texture
    // Process each face group
    for (const auto& [miptexIndex, faceIndices] : facesByTexture) {
        BSPMeshGroup group;
        group.miptexIndex = miptexIndex;
        
        std::vector<renderer::Vertex> renderVertices;
        std::vector<u32> renderIndices;
        
        // Process each face in this texture group
        for (u32 faceIdx : faceIndices) {
            const bsp::BSPFace& face = faces[faceIdx];
            
            if (face.numEdges < 3) continue;
        
            // Get face normal from plane
            // Apply same swap as vertices: GoldSrc (X,Y,Z) -> OpenGL (Z,Y,X)
            // Transformation matrix in render function handles final orientation
            Vec3 faceNormal(0.0f, 1.0f, 0.0f);
            if (face.planeIndex < planeCount) {
                const bsp::BSPPlane& plane = planes[face.planeIndex];
                // Simple swap: GoldSrc (X,Y,Z) -> OpenGL (Z,Y,X)
                faceNormal = Vec3(plane.normal[2], plane.normal[1], plane.normal[0]);
                // Flip normal if face is on back side
                if (face.side != 0) {
                    faceNormal = -faceNormal;
                }
            }
            
            // Get face vertices from edges
            std::vector<u16> faceVertexIndices;
            faceVertexIndices.reserve(face.numEdges);
            
            for (u16 i = 0; i < face.numEdges; ++i) {
                i32 edgeIdx = surfedges[face.firstEdge + i];
                u16 v0, v1;
                
                if (edgeIdx >= 0) {
                    v0 = edges[edgeIdx].vertexIndices[0];
                    v1 = edges[edgeIdx].vertexIndices[1];
                } else {
                    v0 = edges[-edgeIdx].vertexIndices[1];
                    v1 = edges[-edgeIdx].vertexIndices[0];
                }
                
                // Add unique vertices
                if (faceVertexIndices.empty() || faceVertexIndices.back() != v0) {
                    faceVertexIndices.push_back(v0);
                }
                if (faceVertexIndices.empty() || faceVertexIndices.back() != v1) {
                    faceVertexIndices.push_back(v1);
                }
            }
            
            // Triangulate the face (fan triangulation)
            if (faceVertexIndices.size() >= 3) {
                u32 baseIndex = static_cast<u32>(renderVertices.size());
                
                // Calculate texture coordinates from texture info
                bool hasTexCoords = false;
                f32 sAxis[4] = {0.0f};
                f32 tAxis[4] = {0.0f};
                
                if (texInfos != nullptr && face.textureInfo < texInfoCount) {
                    const bsp::BSPTextureInfo& texInfo = texInfos[face.textureInfo];
                    // Texture vectors are stored as vecs[2][4] where:
                    // vecs[0] = S axis (U direction)
                    // vecs[1] = T axis (V direction)
                    // Each is [Sx, Sy, Sz, offset] or [Tx, Ty, Tz, offset]
                    // Apply same swap as vertices: GoldSrc (X,Y,Z) -> OpenGL (Z,Y,X)
                    // Transformation matrix in render function handles final orientation
                    sAxis[0] = texInfo.vecs[0][2];   // GoldSrc Z -> OpenGL X
                    sAxis[1] = texInfo.vecs[0][1];   // GoldSrc Y -> OpenGL Y
                    sAxis[2] = texInfo.vecs[0][0];   // GoldSrc X -> OpenGL Z
                    sAxis[3] = texInfo.vecs[0][3];   // Offset
                    
                    tAxis[0] = texInfo.vecs[1][2];   // GoldSrc Z -> OpenGL X
                    tAxis[1] = texInfo.vecs[1][1];   // GoldSrc Y -> OpenGL Y
                    tAxis[2] = texInfo.vecs[1][0];   // GoldSrc X -> OpenGL Z
                    tAxis[3] = texInfo.vecs[1][3];   // Offset
                    
                    hasTexCoords = true;
                }
                
                // Add vertices with proper normals and texture coordinates
                for (u16 vIdx : faceVertexIndices) {
                    if (vIdx >= vertexCount) continue;
                    
                    renderer::Vertex v;
                    v.position = positions[vIdx];
                    v.normal = faceNormal;  // Use plane normal
                    
                    if (hasTexCoords) {
                        // Calculate texture coordinates from texture vectors
                        // U = dot(position, sAxis) + sAxis[3]
                        // V = dot(position, tAxis) + tAxis[3]
                        // Scale by texture size (typically 64x64 or 128x128 for GoldSrc)
                        f32 u = (v.position.x * sAxis[0] + v.position.y * sAxis[1] + v.position.z * sAxis[2] + sAxis[3]) / 64.0f;
                        f32 texV = (v.position.x * tAxis[0] + v.position.y * tAxis[1] + v.position.z * tAxis[2] + tAxis[3]) / 64.0f;
                        v.texCoord = Vec2(u, texV);
                    } else {
                        v.texCoord = Vec2(0.0f, 0.0f);
                    }
                    
                    renderVertices.push_back(v);
                }
                
                // Create triangles (fan)
                for (u32 i = 1; i + 1 < faceVertexIndices.size(); ++i) {
                    renderIndices.push_back(baseIndex);
                    renderIndices.push_back(baseIndex + i);
                    renderIndices.push_back(baseIndex + i + 1);
                }
            }
        }
        
        // Create mesh for this texture group
        if (!renderVertices.empty()) {
            group.vertices = std::move(renderVertices);
            group.indices = std::move(renderIndices);
            group.mesh.create(group.vertices, group.indices);
            mesh.groups.push_back(std::move(group));
        }
    }
    
    if (mesh.groups.empty()) {
        return std::unexpected(Error{"No valid geometry found in BSP file"});
    }
    
    u32 totalVertices = 0;
    u32 totalIndices = 0;
    for (const auto& group : mesh.groups) {
        totalVertices += static_cast<u32>(group.vertices.size());
        totalIndices += static_cast<u32>(group.indices.size());
    }
    LOG_INFO("Extracted {} mesh groups with {} total vertices and {} total indices from BSP", 
             mesh.groups.size(), totalVertices, totalIndices);
    
    // Calculate map bounding box from all vertices (untransformed)
    AABB untransformedBounds;
    for (const auto& group : mesh.groups) {
        for (const auto& vertex : group.vertices) {
            untransformedBounds.expand(vertex.position);
        }
    }
    
    // Transform bounds using the same transformation applied in render
    // Render applies: 90° Z rotation, 180° Y rotation, Y scale -1
    Mat4 transformMatrix = glm::mat4(1.0f);
    transformMatrix = glm::rotate(transformMatrix, math::radians(90.0f), Vec3(0.0f, 0.0f, 1.0f));
    transformMatrix = glm::rotate(transformMatrix, math::radians(180.0f), Vec3(0.0f, 1.0f, 0.0f));
    transformMatrix = glm::scale(transformMatrix, Vec3(1.0f, -1.0f, 1.0f));
    
    // Transform the bounds to match the rendered map orientation
    mesh.bounds = untransformedBounds.transformed(transformMatrix);
    
    LOG_INFO("Map bounds (untransformed): min=({:.1f}, {:.1f}, {:.1f}), max=({:.1f}, {:.1f}, {:.1f})",
             untransformedBounds.min.x, untransformedBounds.min.y, untransformedBounds.min.z,
             untransformedBounds.max.x, untransformedBounds.max.y, untransformedBounds.max.z);
    LOG_INFO("Map bounds (transformed): min=({:.1f}, {:.1f}, {:.1f}), max=({:.1f}, {:.1f}, {:.1f})",
             mesh.bounds.min.x, mesh.bounds.min.y, mesh.bounds.min.z,
             mesh.bounds.max.x, mesh.bounds.max.y, mesh.bounds.max.z);
    
    // Load textures from BSP (must be done before assigning texture IDs to groups)
    if (texInfos != nullptr && texInfoCount > 0) {
        loadTextures(file, header, mesh, texInfos, texInfoCount, path);
    }
    
    // Assign texture IDs to mesh groups
    u32 groupsWithTextures = 0;
    u32 groupsWithoutTextures = 0;
    for (auto& group : mesh.groups) {
        auto it = mesh.textureMap.find(group.miptexIndex);
        if (it != mesh.textureMap.end()) {
            group.textureID = it->second;
            groupsWithTextures++;
        } else {
            // Texture not loaded (likely in WAD file)
            group.textureID = 0;
            groupsWithoutTextures++;
            LOG_DEBUG("Mesh group with miptex index {} has no loaded texture (likely in WAD file)", group.miptexIndex);
        }
    }
    LOG_INFO("Texture assignment: {} groups have textures, {} groups missing textures (need WAD files)", 
             groupsWithTextures, groupsWithoutTextures);
    
    mesh.loaded = true;
    
    return mesh;
}

bool SimpleBSPLoader::readLump(std::ifstream& file, bsp::BSPLumpType lumpType, 
                                bsp::BSPHeader& header, std::vector<u8>& data) {
    i32 lumpIdx = static_cast<i32>(lumpType);
    if (lumpIdx < 0 || lumpIdx >= 15) {
        LOG_ERROR("Invalid lump type: {}", lumpIdx);
        return false;
    }
    
    const bsp::BSPLump& lump = header.lumps[lumpIdx];
    i32 offset = lump.offset;
    i32 length = lump.length;
    
    LOG_INFO("Reading lump {}: offset={}, length={}", lumpIdx, offset, length);
    
    if (offset <= 0 || length <= 0) {
        LOG_WARN("Lump {} has invalid offset/length: offset={}, length={}", lumpIdx, offset, length);
        return false;
    }
    
    file.seekg(offset, std::ios::beg);
    if (!file.good()) {
        LOG_ERROR("Failed to seek to lump {} offset {}", lumpIdx, offset);
        return false;
    }
    
    data.resize(length);
    file.read(reinterpret_cast<char*>(data.data()), length);
    
    i32 bytesRead = static_cast<i32>(file.gcount());
    if (!file.good() || bytesRead != length) {
        LOG_ERROR("Failed to read lump {}: expected {} bytes, got {}", lumpIdx, length, bytesRead);
        return false;
    }
    
    LOG_INFO("Successfully read lump {}: {} bytes", lumpIdx, length);
    return true;
}

SimpleBSPMesh SimpleBSPLoader::createTestMesh() {
    SimpleBSPMesh mesh;
    
    // Create a simple floor/room for testing
    BSPMeshGroup group;
    group.miptexIndex = -1;  // No texture
    group.textureID = 0;
    
    f32 size = 500.0f;
    group.vertices.push_back({{-size, 0, -size}, {0, 1, 0}, {0, 0}});
    group.vertices.push_back({{ size, 0, -size}, {0, 1, 0}, {1, 0}});
    group.vertices.push_back({{ size, 0,  size}, {0, 1, 0}, {1, 1}});
    group.vertices.push_back({{-size, 0,  size}, {0, 1, 0}, {0, 1}});
    
    group.indices.push_back(0); group.indices.push_back(1); group.indices.push_back(2);
    group.indices.push_back(0); group.indices.push_back(2); group.indices.push_back(3);
    
    // Create OpenGL mesh
    group.mesh.create(group.vertices, group.indices);
    mesh.groups.push_back(std::move(group));
    mesh.loaded = true;
    
    LOG_INFO("Created test mesh with {} vertices, {} indices", 
             mesh.groups[0].vertices.size(), mesh.groups[0].indices.size());
    
    return mesh;
}

bool SimpleBSPLoader::loadTextures(std::ifstream& file, const bsp::BSPHeader& header, 
                                    SimpleBSPMesh& mesh, const bsp::BSPTextureInfo* texInfos, u32 texInfoCount,
                                    const std::string& path) {
    (void)texInfos;      // Will be used for WAD loading later
    (void)texInfoCount;  // Will be used for WAD loading later
    
    // Read texture lump
    std::vector<u8> textureData;
    bsp::BSPHeader nonConstHeader = header;  // Make a copy for readLump
    if (!readLump(file, bsp::LUMP_TEXTURES, nonConstHeader, textureData)) {
        LOG_WARN("Failed to read texture lump, textures will not be available");
        return false;
    }
    
    LOG_INFO("Texture lump size: {} bytes", textureData.size());
    
    if (textureData.empty()) {
        LOG_WARN("Texture lump is empty");
        return false;
    }
    
    // First 4 bytes: number of textures
    const i32* numTexturesPtr = reinterpret_cast<const i32*>(textureData.data());
    i32 numTextures = *numTexturesPtr;
    
    if (numTextures <= 0 || numTextures > 1024) {  // Sanity check
        LOG_WARN("Invalid texture count: {}", numTextures);
        return false;
    }
    
    LOG_INFO("BSP contains {} textures", numTextures);
    
    // Array of offsets follows
    const i32* offsets = reinterpret_cast<const i32*>(textureData.data() + sizeof(i32));
    
    // Load each texture
    // Map miptex index to OpenGL texture ID
    for (i32 i = 0; i < numTextures; ++i) {
        if (offsets[i] == -1) {
            // Empty texture slot - texture is in WAD file, not embedded
            LOG_DEBUG("Texture slot {} is empty (likely in WAD file)", i);
            continue;
        }
        
        // Read miptex header
        const bsp::BSPMiptex* miptex = reinterpret_cast<const bsp::BSPMiptex*>(
            textureData.data() + offsets[i]
        );
        
        if (miptex->width == 0 || miptex->height == 0 || 
            miptex->width > 1024 || miptex->height > 1024) {
            LOG_WARN("Invalid texture dimensions: {}x{}", miptex->width, miptex->height);
            continue;
        }
        
        // Get pixel data from first mip level (full resolution)
        // Offsets are relative to the start of the miptex structure
        if (miptex->offsets[0] == 0) {
            LOG_DEBUG("Texture {} '{}' has no embedded pixel data (likely in WAD file)", i, miptex->name);
            continue;
        }
        
        // Calculate absolute offset: miptex pointer + offset
        const u8* miptexBase = reinterpret_cast<const u8*>(miptex);
        const u8* pixelData = miptexBase + miptex->offsets[0];
        u32 pixelCount = miptex->width * miptex->height;
        
        // Verify we have enough data
        u32 expectedDataSize = pixelCount;  // 8-bit indexed color
        if (miptex->offsets[0] + expectedDataSize > textureData.size()) {
            LOG_WARN("Texture {} '{}' pixel data extends beyond texture lump", i, miptex->name);
            continue;
        }
        
        // Create OpenGL texture
        u32 textureID = createTextureFromMiptex(*miptex, pixelData, pixelCount);
        if (textureID != 0) {
            // Map by miptex index (this is what BSPTextureInfo.miptex refers to)
            mesh.textureMap[i] = textureID;
            LOG_INFO("Loaded embedded texture {}: '{}' {}x{} (ID: {})", 
                     i, miptex->name, miptex->width, miptex->height, textureID);
        }
    }
    
    LOG_INFO("Loaded {} embedded textures from BSP (out of {} total)", mesh.textureMap.size(), numTextures);
    
    // Collect texture names and miptex indices for WAD loading
    // Map: texture name -> miptex index
    std::unordered_map<std::string, i32> textureNameToIndex;
    std::vector<std::string> missingTextureNames;
    
    for (i32 i = 0; i < numTextures; ++i) {
        if (offsets[i] == -1) {
            // Empty texture slot - skip
            continue;
        }
        
        // Read miptex header to get texture name
        const bsp::BSPMiptex* miptex = reinterpret_cast<const bsp::BSPMiptex*>(
            textureData.data() + offsets[i]
        );
        
        std::string texName(miptex->name, 16);
        // Trim null characters
        texName = texName.c_str();
        
        textureNameToIndex[texName] = i;
        
        // If texture wasn't loaded (no embedded data), add to missing list
        if (mesh.textureMap.find(i) == mesh.textureMap.end()) {
            missingTextureNames.push_back(texName);
        }
    }
    
    // Load missing textures from WAD files
    if (!missingTextureNames.empty()) {
        LOG_INFO("Attempting to load {} missing textures from WAD files", missingTextureNames.size());
        
        // Try to find BSP file directory
        std::string bspDir = path;
        size_t lastSlash = bspDir.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            bspDir = bspDir.substr(0, lastSlash + 1);
        } else {
            bspDir = "";
        }
        
        // Try common WAD file names
        std::vector<std::string> wadFiles = {
            bspDir + "halflife.wad",
            bspDir + "decals.wad",
            bspDir + "cs_dust.wad",
            "assets/maps/halflife.wad",
            "assets/maps/decals.wad",
            "assets/maps/cs_dust.wad"
        };
        
        loadWADTextures(path, mesh, missingTextureNames, textureNameToIndex);
    }
    
    return true;
}

// Loaded palette from palette.lmp file
static u8 g_palette[256][3] = {0};
static bool g_paletteLoaded = false;

// Load palette from palette.lmp file (256 colors * 3 bytes RGB = 768 bytes)
static bool loadPalette() {
    if (g_paletteLoaded) {
        return true;
    }
    
    // Try multiple paths to find palette.lmp
    std::vector<std::string> tryPaths = {
        "assets/gfx/palette.lmp",
        "../assets/gfx/palette.lmp",
        "../../assets/gfx/palette.lmp",
        "../../../assets/gfx/palette.lmp",
        "gfx/palette.lmp"
    };
    
    std::string foundPath;
    std::ifstream file;
    for (const auto& path : tryPaths) {
        file.open(path, std::ios::binary);
        if (file.is_open()) {
            foundPath = path;
            break;
        }
    }
    
    if (!file.is_open()) {
        LOG_ERROR("Failed to find palette.lmp file. Tried: {}", tryPaths[0]);
        return false;
    }
    
    // Read palette data (768 bytes: 256 colors * 3 bytes RGB)
    file.read(reinterpret_cast<char*>(g_palette), 256 * 3);
    
    if (!file.good() || file.gcount() != 768) {
        LOG_ERROR("Failed to read palette.lmp: expected 768 bytes, got {}", file.gcount());
        file.close();
        return false;
    }
    
    file.close();
    g_paletteLoaded = true;
    LOG_INFO("Loaded palette from: {}", foundPath);
    return true;
}

u32 SimpleBSPLoader::createTextureFromMiptex(const bsp::BSPMiptex& miptex, const u8* data, u32 dataSize) {
    // Load palette if not already loaded
    if (!loadPalette()) {
        LOG_WARN("Using fallback grayscale conversion - palette not loaded");
    }
    u32 textureID = 0;
    glGenTextures(1, &textureID);
    if (textureID == 0) {
        LOG_ERROR("Failed to generate texture");
        return 0;
    }
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // GoldSrc textures are 8-bit indexed color (palette-based)
    // Convert indexed color to RGB using the standard Quake/Half-Life palette
    std::vector<u8> rgbData(miptex.width * miptex.height * 3);
    u32 pixelCount = miptex.width * miptex.height;
    
    // Check if we have enough data
    if (dataSize < pixelCount) {
        LOG_WARN("Texture data size {} is less than expected {} pixels", dataSize, pixelCount);
    }
    
    // Convert indexed colors to RGB using the palette
    for (u32 i = 0; i < pixelCount && i < dataSize; ++i) {
        u8 index = data[i];
        
        // Use the palette to get RGB values
        // Index 255 is typically transparent, but we'll render it as the palette color
        if (g_paletteLoaded) {
            const u8* paletteColor = g_palette[index];
            rgbData[i * 3 + 0] = paletteColor[0];  // R
            rgbData[i * 3 + 1] = paletteColor[1];  // G
            rgbData[i * 3 + 2] = paletteColor[2];  // B
        } else {
            // Fallback: grayscale conversion if palette not loaded
            rgbData[i * 3 + 0] = index;
            rgbData[i * 3 + 1] = index;
            rgbData[i * 3 + 2] = index;
        } 
    }
    
    // Verify texture has non-zero data
    u32 nonZeroPixels = 0;
    for (u32 i = 0; i < pixelCount; ++i) {
        if (rgbData[i * 3] > 0 || rgbData[i * 3 + 1] > 0 || rgbData[i * 3 + 2] > 0) {
            nonZeroPixels++;
        }
    }
    
    // Log sample pixels for debugging
    if (pixelCount > 10) {
        LOG_INFO("Texture '{}' - non-zero pixels: {}/{} ({:.1f}%), sample - first: index={}, RGB=({},{},{}), middle: index={}, RGB=({},{},{})", 
                 miptex.name, nonZeroPixels, pixelCount, (100.0f * nonZeroPixels / pixelCount),
                 data[0], rgbData[0], rgbData[1], rgbData[2],
                 data[pixelCount/2], rgbData[(pixelCount/2)*3], rgbData[(pixelCount/2)*3+1], rgbData[(pixelCount/2)*3+2]);
    }
    
    if (nonZeroPixels == 0) {
        LOG_WARN("Texture '{}' has no non-zero pixels! Texture may appear black.", miptex.name);
    }
    
    // Set pixel storage parameters for optimal quality
    // Ensure proper alignment for RGB data (3 bytes per pixel)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // No padding needed for RGB
    
    // Upload texture data with explicit internal format for better quality
    // Use GL_RGB8 instead of GL_RGB for explicit 8-bit per channel
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, miptex.width, miptex.height, 0, 
                 GL_RGB, GL_UNSIGNED_BYTE, rgbData.data());
    
    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_ERROR("OpenGL error uploading texture '{}': 0x{:X}", miptex.name, static_cast<u32>(err));
    }
    
    // Generate high-quality mipmaps for better quality when textures are viewed at distance
    // This creates multiple resolution levels automatically
    glGenerateMipmap(GL_TEXTURE_2D);
    
    err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_WARN("OpenGL error generating mipmaps for '{}': 0x{:X}", miptex.name, static_cast<u32>(err));
    }
    
    // Set texture parameters for maximum quality filtering
    // Use trilinear filtering (GL_LINEAR_MIPMAP_LINEAR) for minification - smooth interpolation between mip levels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // Use linear filtering for magnification (when viewing textures up close)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Repeat texture coordinates (seamless tiling)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Set LOD bias to slightly prefer higher quality mip levels (sharper textures)
    // Negative values = prefer higher quality (closer to base mip level)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.5f);
    
    // Enable maximum anisotropic filtering if available (reduces blurriness on angled surfaces)
    // This significantly improves texture quality on surfaces viewed at angles
    // Anisotropic filtering is widely supported and part of OpenGL core since 4.6
    GLfloat maxAnisotropy = 1.0f;
    // GL_MAX_TEXTURE_MAX_ANISOTROPY is 0x84FF, but GLAD should define it
    // If not defined, we'll skip anisotropic filtering (textures will still look good with mipmaps)
    #ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
        #define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
        #define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
    #endif
    
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
    if (maxAnisotropy > 1.0f) {
        // Use maximum available anisotropy for best quality (typically 8x or 16x)
        // This provides the sharpest textures on angled surfaces
        GLfloat anisotropy = maxAnisotropy;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
        LOG_DEBUG("Texture '{}' using {}x anisotropic filtering", miptex.name, static_cast<int>(anisotropy));
    }
    
    // Verify texture was created correctly
    GLint width = 0, height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    if (width != static_cast<GLint>(miptex.width) || height != static_cast<GLint>(miptex.height)) {
        LOG_ERROR("Texture '{}' size mismatch! Expected: {}x{}, Got: {}x{}", 
                 miptex.name, miptex.width, miptex.height, width, height);
    } else {
        LOG_DEBUG("Texture '{}' verified: {}x{}", miptex.name, width, height);
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return textureID;
}

bool SimpleBSPLoader::loadWADTextures(const std::string& bspPath, SimpleBSPMesh& mesh, 
                                      const std::vector<std::string>& textureNames,
                                      const std::unordered_map<std::string, i32>& textureNameToIndex) {
    // Extract directory from BSP path
    std::string bspDir = bspPath;
    size_t lastSlash = bspDir.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        bspDir = bspDir.substr(0, lastSlash + 1);
    } else {
        bspDir = "";
    }
    
    // Try common WAD file names
    std::vector<std::string> wadFiles = {
        bspDir + "halflife.wad",
        bspDir + "decals.wad",
        bspDir + "cs_dust.wad",
        "assets/maps/halflife.wad",
        "assets/maps/decals.wad",
        "assets/maps/cs_dust.wad"
    };
    
    u32 totalLoaded = 0;
    for (const auto& wadPath : wadFiles) {
        u32 loaded = loadWADFile(wadPath, mesh, textureNames, textureNameToIndex);
        totalLoaded += loaded;
    }
    
    if (totalLoaded > 0) {
        LOG_INFO("Loaded {} textures from WAD files", totalLoaded);
    } else {
        LOG_WARN("No textures loaded from WAD files. Tried: halflife.wad, decals.wad, cs_dust.wad");
    }
    
    return totalLoaded > 0;
}

bool SimpleBSPLoader::loadWADFile(const std::string& wadPath, SimpleBSPMesh& mesh, 
                                   const std::vector<std::string>& neededTextures,
                                   const std::unordered_map<std::string, i32>& textureNameToIndex) {
    std::ifstream file(wadPath, std::ios::binary);
    if (!file.is_open()) {
        LOG_DEBUG("WAD file not found: {}", wadPath);
        return false;
    }
    
    LOG_INFO("Loading WAD file: {}", wadPath);
    
    // Read WAD header
    bsp::WADHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!file.good() || file.gcount() != sizeof(header)) {
        LOG_WARN("Failed to read WAD header from {}", wadPath);
        return false;
    }
    
    // Check magic number
    if (std::memcmp(header.magic, "WAD3", 4) != 0) {
        LOG_WARN("Invalid WAD magic number in {} (expected WAD3)", wadPath);
        return false;
    }
    
    if (header.numEntries <= 0 || header.numEntries > 10000) {
        LOG_WARN("Invalid WAD entry count: {}", header.numEntries);
        return false;
    }
    
    // Read directory
    file.seekg(header.dirOffset, std::ios::beg);
    std::vector<bsp::WADEntry> entries(header.numEntries);
    file.read(reinterpret_cast<char*>(entries.data()), 
              header.numEntries * sizeof(bsp::WADEntry));
    
    if (!file.good() || file.gcount() != static_cast<std::streamsize>(header.numEntries * sizeof(bsp::WADEntry))) {
        LOG_WARN("Failed to read WAD directory from {}", wadPath);
        return false;
    }
    
    // Create a set of needed texture names for fast lookup
    std::unordered_set<std::string> neededSet(neededTextures.begin(), neededTextures.end());
    
    u32 loadedCount = 0;
    
    // Search for needed textures in WAD
    for (const auto& entry : entries) {
        if (entry.type != 0x43) {  // 0x43 = miptex
            continue;
        }
        
        std::string texName(entry.name, 16);
        texName = texName.c_str();  // Trim null characters
        
        // Check if we need this texture
        if (neededSet.find(texName) == neededSet.end()) {
            continue;
        }
        
        // Find the miptex index for this texture name
        auto it = textureNameToIndex.find(texName);
        if (it == textureNameToIndex.end()) {
            LOG_DEBUG("Texture '{}' found in WAD but not in BSP texture name map", texName);
            continue;
        }
        
        i32 miptexIndex = it->second;
        
        // Skip if we already have this texture loaded
        if (mesh.textureMap.find(miptexIndex) != mesh.textureMap.end()) {
            continue;
        }
        
        // Read miptex data
        file.seekg(entry.offset, std::ios::beg);
        bsp::BSPMiptex miptex;
        file.read(reinterpret_cast<char*>(&miptex), sizeof(miptex));
        
        if (!file.good() || file.gcount() != sizeof(miptex)) {
            LOG_WARN("Failed to read miptex header for '{}' from {}", texName, wadPath);
            continue;
        }
        
        if (miptex.width == 0 || miptex.height == 0 || 
            miptex.width > 1024 || miptex.height > 1024) {
            LOG_WARN("Invalid texture dimensions for '{}': {}x{}", texName, miptex.width, miptex.height);
            continue;
        }
        
        // Read pixel data (first mip level)
        u32 pixelCount = miptex.width * miptex.height;
        std::vector<u8> pixelData(pixelCount);
        
        if (miptex.offsets[0] > 0) {
            file.seekg(entry.offset + miptex.offsets[0], std::ios::beg);
            file.read(reinterpret_cast<char*>(pixelData.data()), pixelCount);
            
            if (!file.good() || file.gcount() != static_cast<std::streamsize>(pixelCount)) {
                LOG_WARN("Failed to read pixel data for '{}' from {}", texName, wadPath);
                continue;
            }
            
            // Create OpenGL texture
            u32 textureID = createTextureFromMiptex(miptex, pixelData.data(), pixelCount);
            if (textureID != 0) {
                // Map texture to miptex index
                mesh.textureMap[miptexIndex] = textureID;
                LOG_INFO("Loaded texture '{}' from WAD {} (miptex index {}, {}x{}, ID: {})", 
                         texName, wadPath, miptexIndex, miptex.width, miptex.height, textureID);
                loadedCount++;
            }
        } else {
            LOG_WARN("Texture '{}' in WAD has no pixel data", texName);
        }
    }
    
    if (loadedCount > 0) {
        LOG_INFO("Loaded {} textures from WAD file {}", loadedCount, wadPath);
    }
    
    return loadedCount > 0;
}

} // namespace cscpp::assets

