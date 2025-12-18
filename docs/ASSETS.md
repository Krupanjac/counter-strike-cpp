# Asset Pipeline Documentation

## Overview

The asset pipeline uses glTF 2.0 as the canonical runtime format, following Khronos Asset Creation Guidelines 2.0. Optional BSP support allows loading original GoldSrc maps.

## Asset Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│                        CREATION PHASE                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐                    │
│   │ Blender │    │ ZBrush  │    │Substance│                    │
│   │ (Mesh)  │    │ (Sculpt)│    │(Texture)│                    │
│   └────┬────┘    └────┬────┘    └────┬────┘                    │
│        │              │              │                          │
│        └──────────────┼──────────────┘                          │
│                       │                                          │
│                       ▼                                          │
│               ┌───────────────┐                                 │
│               │  Blender      │                                 │
│               │  glTF Export  │                                 │
│               └───────┬───────┘                                 │
│                       │                                          │
└───────────────────────┼──────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────────┐
│                     PROCESSING PHASE                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│               ┌───────────────┐                                 │
│               │    Asset      │                                 │
│               │   Compiler    │                                 │
│               └───────┬───────┘                                 │
│                       │                                          │
│         ┌─────────────┼─────────────┐                           │
│         │             │             │                           │
│         ▼             ▼             ▼                           │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐                       │
│   │ Validate │ │ Compress │ │ Generate │                       │
│   │   glTF   │ │ Textures │ │   LODs   │                       │
│   │          │ │ (KTX2)   │ │          │                       │
│   └──────────┘ └──────────┘ └──────────┘                       │
│                       │                                          │
│                       ▼                                          │
│               ┌───────────────┐                                 │
│               │   Packaged    │                                 │
│               │    Asset      │                                 │
│               │   (.cscpak)   │                                 │
│               └───────────────┘                                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────────┐
│                      RUNTIME PHASE                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│               ┌───────────────┐                                 │
│               │    Asset      │                                 │
│               │   Streamer    │                                 │
│               └───────┬───────┘                                 │
│                       │                                          │
│         ┌─────────────┼─────────────┐                           │
│         │             │             │                           │
│         ▼             ▼             ▼                           │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐                       │
│   │  Mesh    │ │ Texture  │ │Animation │                       │
│   │  Cache   │ │  Cache   │ │  Cache   │                       │
│   └──────────┘ └──────────┘ └──────────┘                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## glTF 2.0 Integration

### Supported Extensions

| Extension | Purpose |
|-----------|---------|
| KHR_materials_pbrSpecularGlossiness | Legacy PBR support |
| KHR_texture_transform | Texture UV transforms |
| KHR_mesh_quantization | Compressed vertex data |
| KHR_draco_mesh_compression | Mesh compression |
| KHR_texture_basisu | KTX2/Basis Universal textures |
| KHR_lights_punctual | Point/spot/directional lights |

### Loading with tinygltf

```cpp
#include <tiny_gltf.h>

class GLTFLoader {
public:
    std::optional<ModelAsset> load(const std::string& path) {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        
        bool success = false;
        if (path.ends_with(".glb")) {
            success = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        } else {
            success = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        }
        
        if (!warn.empty()) {
            LOG_WARN("glTF warning: {}", warn);
        }
        
        if (!err.empty() || !success) {
            LOG_ERROR("glTF error: {}", err);
            return std::nullopt;
        }
        
        return processModel(model);
    }
    
private:
    ModelAsset processModel(const tinygltf::Model& model) {
        ModelAsset asset;
        
        // Process materials first
        for (const auto& mat : model.materials) {
            asset.materials.push_back(processMaterial(model, mat));
        }
        
        // Process meshes
        for (const auto& mesh : model.meshes) {
            asset.meshes.push_back(processMesh(model, mesh));
        }
        
        // Process animations
        for (const auto& anim : model.animations) {
            asset.animations.push_back(processAnimation(model, anim));
        }
        
        // Process scene hierarchy
        if (!model.scenes.empty()) {
            const auto& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];
            for (int nodeIdx : scene.nodes) {
                processNode(model, nodeIdx, glm::mat4(1.0f), asset);
            }
        }
        
        return asset;
    }
    
    MeshAsset processMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh) {
        MeshAsset asset;
        
        for (const auto& primitive : mesh.primitives) {
            // Indices
            if (primitive.indices >= 0) {
                const auto& accessor = model.accessors[primitive.indices];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                
                // Extract indices...
            }
            
            // Positions (required)
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt != primitive.attributes.end()) {
                extractAttribute<glm::vec3>(model, posIt->second, asset.positions);
            }
            
            // Normals
            auto normIt = primitive.attributes.find("NORMAL");
            if (normIt != primitive.attributes.end()) {
                extractAttribute<glm::vec3>(model, normIt->second, asset.normals);
            }
            
            // Tangents
            auto tanIt = primitive.attributes.find("TANGENT");
            if (tanIt != primitive.attributes.end()) {
                extractAttribute<glm::vec4>(model, tanIt->second, asset.tangents);
            }
            
            // UV coordinates
            auto uvIt = primitive.attributes.find("TEXCOORD_0");
            if (uvIt != primitive.attributes.end()) {
                extractAttribute<glm::vec2>(model, uvIt->second, asset.uvs);
            }
            
            // Skinning data
            auto jointsIt = primitive.attributes.find("JOINTS_0");
            auto weightsIt = primitive.attributes.find("WEIGHTS_0");
            if (jointsIt != primitive.attributes.end() && weightsIt != primitive.attributes.end()) {
                // Extract joint indices and weights...
            }
        }
        
        // Calculate bounding box
        asset.boundingBox = calculateAABB(asset.positions);
        
        return asset;
    }
    
    MaterialAsset processMaterial(const tinygltf::Model& model, const tinygltf::Material& mat) {
        MaterialAsset asset;
        asset.name = mat.name;
        
        // PBR Metallic-Roughness
        const auto& pbr = mat.pbrMetallicRoughness;
        
        asset.material.baseColorFactor = glm::vec4(
            pbr.baseColorFactor[0],
            pbr.baseColorFactor[1],
            pbr.baseColorFactor[2],
            pbr.baseColorFactor[3]
        );
        
        asset.material.metallicFactor = static_cast<float>(pbr.metallicFactor);
        asset.material.roughnessFactor = static_cast<float>(pbr.roughnessFactor);
        
        // Textures
        if (pbr.baseColorTexture.index >= 0) {
            asset.albedoTextureIndex = getTextureIndex(model, pbr.baseColorTexture.index);
        }
        
        if (pbr.metallicRoughnessTexture.index >= 0) {
            asset.metallicRoughnessTextureIndex = getTextureIndex(model, pbr.metallicRoughnessTexture.index);
        }
        
        if (mat.normalTexture.index >= 0) {
            asset.normalTextureIndex = getTextureIndex(model, mat.normalTexture.index);
            asset.material.normalScale = static_cast<float>(mat.normalTexture.scale);
        }
        
        if (mat.occlusionTexture.index >= 0) {
            asset.aoTextureIndex = getTextureIndex(model, mat.occlusionTexture.index);
        }
        
        if (mat.emissiveTexture.index >= 0) {
            asset.emissiveTextureIndex = getTextureIndex(model, mat.emissiveTexture.index);
        }
        
        asset.material.emissiveFactor = glm::vec3(
            mat.emissiveFactor[0],
            mat.emissiveFactor[1],
            mat.emissiveFactor[2]
        );
        
        // Alpha mode
        if (mat.alphaMode == "OPAQUE") {
            asset.material.alphaMode = PBRMaterial::AlphaMode::Opaque;
        } else if (mat.alphaMode == "MASK") {
            asset.material.alphaMode = PBRMaterial::AlphaMode::Mask;
            asset.material.alphaCutoff = static_cast<float>(mat.alphaCutoff);
        } else if (mat.alphaMode == "BLEND") {
            asset.material.alphaMode = PBRMaterial::AlphaMode::Blend;
        }
        
        asset.material.doubleSided = mat.doubleSided;
        
        return asset;
    }
};
```

## Texture Pipeline

### KTX2 Format

Using Basis Universal compression for efficient GPU textures:

```cpp
class TextureLoader {
public:
    std::optional<TextureAsset> loadKTX2(const std::string& path) {
        ktxTexture2* texture = nullptr;
        KTX_error_code result = ktxTexture2_CreateFromNamedFile(
            path.c_str(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &texture
        );
        
        if (result != KTX_SUCCESS) {
            LOG_ERROR("Failed to load KTX2: {}", path);
            return std::nullopt;
        }
        
        // Transcode if Basis compressed
        if (ktxTexture2_NeedsTranscoding(texture)) {
            // Choose best format for current GPU
            ktx_transcode_fmt_e targetFormat = selectTargetFormat();
            result = ktxTexture2_TranscodeBasis(texture, targetFormat, 0);
            
            if (result != KTX_SUCCESS) {
                ktxTexture_Destroy(ktxTexture(texture));
                return std::nullopt;
            }
        }
        
        TextureAsset asset;
        asset.width = texture->baseWidth;
        asset.height = texture->baseHeight;
        asset.mipLevels = texture->numLevels;
        asset.format = ktxToGLFormat(texture->vkFormat);
        
        // Upload to GPU
        GLuint glTexture = 0;
        result = ktxTexture_GLUpload(ktxTexture(texture), &glTexture);
        
        ktxTexture_Destroy(ktxTexture(texture));
        
        if (result == KTX_SUCCESS) {
            asset.gpuHandle = glTexture;
            return asset;
        }
        
        return std::nullopt;
    }
    
private:
    ktx_transcode_fmt_e selectTargetFormat() {
        // Check GPU capabilities
        if (GLAD_GL_EXT_texture_compression_s3tc) {
            return KTX_TTF_BC7_RGBA;  // Best quality
        }
        if (GLAD_GL_EXT_texture_compression_bptc) {
            return KTX_TTF_BC7_RGBA;
        }
        // Fallback
        return KTX_TTF_RGBA32;
    }
};
```

### Texture Compression Pipeline

```cpp
class AssetCompiler {
public:
    void compressTexture(const std::string& inputPath, const std::string& outputPath) {
        // Load source image
        int width, height, channels;
        uint8_t* data = stbi_load(inputPath.c_str(), &width, &height, &channels, 4);
        
        if (!data) {
            throw std::runtime_error("Failed to load: " + inputPath);
        }
        
        // Create KTX2 texture
        ktxTextureCreateInfo createInfo = {};
        createInfo.glInternalformat = GL_RGBA8;
        createInfo.vkFormat = VK_FORMAT_R8G8B8A8_UNORM;
        createInfo.baseWidth = width;
        createInfo.baseHeight = height;
        createInfo.baseDepth = 1;
        createInfo.numDimensions = 2;
        createInfo.numLevels = calculateMipLevels(width, height);
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.generateMipmaps = KTX_TRUE;
        
        ktxTexture2* texture = nullptr;
        ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
        
        // Set base level data
        ktxTexture_SetImageFromMemory(
            ktxTexture(texture), 0, 0, 0,
            data, width * height * 4
        );
        
        stbi_image_free(data);
        
        // Generate mipmaps
        // ... (use a mipmap generation library)
        
        // Compress with Basis Universal
        ktxBasisParams params = {};
        params.structSize = sizeof(params);
        params.uastc = KTX_TRUE;  // UASTC for higher quality
        params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
        
        ktxTexture2_CompressBasisEx(texture, &params);
        
        // Write to file
        ktxTexture_WriteToNamedFile(ktxTexture(texture), outputPath.c_str());
        ktxTexture_Destroy(ktxTexture(texture));
    }
};
```

## BSP Map Loading

### GoldSrc BSP Format

```cpp
// BSP file header
struct BSPHeader {
    int32_t version;  // 30 for GoldSrc
    BSPLump lumps[15];
};

struct BSPLump {
    int32_t offset;
    int32_t length;
};

enum BSPLumpIndex {
    LUMP_ENTITIES = 0,
    LUMP_PLANES = 1,
    LUMP_TEXTURES = 2,
    LUMP_VERTICES = 3,
    LUMP_VISIBILITY = 4,
    LUMP_NODES = 5,
    LUMP_TEXINFO = 6,
    LUMP_FACES = 7,
    LUMP_LIGHTING = 8,
    LUMP_CLIPNODES = 9,
    LUMP_LEAVES = 10,
    LUMP_MARKSURFACES = 11,
    LUMP_EDGES = 12,
    LUMP_SURFEDGES = 13,
    LUMP_MODELS = 14,
};

// Key structures
struct BSPPlane {
    glm::vec3 normal;
    float dist;
    int32_t type;
};

struct BSPNode {
    int32_t planeIndex;
    int16_t children[2];  // Negative = leaf index
    int16_t mins[3];
    int16_t maxs[3];
    uint16_t firstFace;
    uint16_t numFaces;
};

struct BSPLeaf {
    int32_t contents;
    int32_t visOffset;
    int16_t mins[3];
    int16_t maxs[3];
    uint16_t firstMarkSurface;
    uint16_t numMarkSurfaces;
    uint8_t ambientLevels[4];
};

struct BSPFace {
    int16_t planeIndex;
    int16_t side;
    int32_t firstEdge;
    int16_t numEdges;
    int16_t texInfo;
    uint8_t styles[4];
    int32_t lightOffset;
};

struct BSPClipNode {
    int32_t planeIndex;
    int16_t children[2];
};
```

### BSP Loader Implementation

```cpp
class BSPLoader {
public:
    bool load(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Read header
        BSPHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        
        if (header.version != 30) {
            LOG_ERROR("Unsupported BSP version: {}", header.version);
            return false;
        }
        
        // Load lumps
        loadLump(file, header.lumps[LUMP_PLANES], m_planes);
        loadLump(file, header.lumps[LUMP_VERTICES], m_vertices);
        loadLump(file, header.lumps[LUMP_NODES], m_nodes);
        loadLump(file, header.lumps[LUMP_LEAVES], m_leaves);
        loadLump(file, header.lumps[LUMP_FACES], m_faces);
        loadLump(file, header.lumps[LUMP_EDGES], m_edges);
        loadLump(file, header.lumps[LUMP_SURFEDGES], m_surfEdges);
        loadLump(file, header.lumps[LUMP_CLIPNODES], m_clipNodes);
        
        // Load textures (WAD-based)
        loadTextures(file, header.lumps[LUMP_TEXTURES]);
        
        // Load visibility data
        loadVisibility(file, header.lumps[LUMP_VISIBILITY]);
        
        // Parse entities
        loadEntities(file, header.lumps[LUMP_ENTITIES]);
        
        // Build renderable geometry
        buildRenderMeshes();
        
        return true;
    }
    
    // Collision trace through BSP
    TraceResult traceLine(glm::vec3 start, glm::vec3 end, int hullIndex = 0) const {
        TraceResult result;
        result.fraction = 1.0f;
        result.endpos = end;
        result.allsolid = false;
        result.startsolid = false;
        
        recursiveHullCheck(
            hullIndex == 0 ? m_nodes.data() : nullptr,
            m_clipNodes.data(),
            0,  // Start at root
            0.0f, 1.0f,
            start, end,
            result
        );
        
        if (result.fraction < 1.0f) {
            result.endpos = start + (end - start) * result.fraction;
        }
        
        return result;
    }
    
private:
    template<typename T>
    void loadLump(std::ifstream& file, const BSPLump& lump, std::vector<T>& data) {
        size_t count = lump.length / sizeof(T);
        data.resize(count);
        file.seekg(lump.offset);
        file.read(reinterpret_cast<char*>(data.data()), lump.length);
    }
    
    void recursiveHullCheck(
        const BSPNode* nodes,
        const BSPClipNode* clipNodes,
        int nodeIndex,
        float p1f, float p2f,
        glm::vec3 p1, glm::vec3 p2,
        TraceResult& result
    ) const {
        if (nodeIndex < 0) {
            // Leaf node
            int leafIndex = -nodeIndex - 1;
            if (m_leaves[leafIndex].contents == CONTENTS_SOLID) {
                if (result.fraction > p1f) {
                    result.fraction = p1f;
                    result.startsolid = (p1f == 0.0f);
                }
            }
            return;
        }
        
        // Get split plane
        const BSPPlane& plane = m_planes[clipNodes[nodeIndex].planeIndex];
        
        float t1 = glm::dot(plane.normal, p1) - plane.dist;
        float t2 = glm::dot(plane.normal, p2) - plane.dist;
        
        if (t1 >= 0 && t2 >= 0) {
            // Both on front
            recursiveHullCheck(nodes, clipNodes, clipNodes[nodeIndex].children[0],
                              p1f, p2f, p1, p2, result);
        } else if (t1 < 0 && t2 < 0) {
            // Both on back
            recursiveHullCheck(nodes, clipNodes, clipNodes[nodeIndex].children[1],
                              p1f, p2f, p1, p2, result);
        } else {
            // Split
            int side = t1 < 0 ? 1 : 0;
            float frac = t1 / (t1 - t2);
            frac = std::clamp(frac, 0.0f, 1.0f);
            
            float midf = p1f + (p2f - p1f) * frac;
            glm::vec3 mid = p1 + (p2 - p1) * frac;
            
            recursiveHullCheck(nodes, clipNodes, clipNodes[nodeIndex].children[side],
                              p1f, midf, p1, mid, result);
            recursiveHullCheck(nodes, clipNodes, clipNodes[nodeIndex].children[!side],
                              midf, p2f, mid, p2, result);
        }
    }
    
    std::vector<BSPPlane> m_planes;
    std::vector<glm::vec3> m_vertices;
    std::vector<BSPNode> m_nodes;
    std::vector<BSPLeaf> m_leaves;
    std::vector<BSPFace> m_faces;
    std::vector<BSPEdge> m_edges;
    std::vector<int32_t> m_surfEdges;
    std::vector<BSPClipNode> m_clipNodes;
};
```

## Asset Streaming

### Streaming System

```cpp
class AssetStreamer {
public:
    void update(const glm::vec3& cameraPosition) {
        // Determine which assets should be loaded based on distance
        for (auto& [id, asset] : m_registeredAssets) {
            float distance = glm::distance(cameraPosition, asset.position);
            
            if (distance < asset.loadDistance && !asset.loaded) {
                queueLoad(id, asset);
            } else if (distance > asset.unloadDistance && asset.loaded) {
                queueUnload(id);
            }
        }
        
        // Process load queue on worker thread
        processLoadQueue();
    }
    
    void queueLoad(AssetId id, AssetInfo& info) {
        if (info.loading) return;
        
        info.loading = true;
        m_loadQueue.push({id, info.path, info.type});
    }
    
private:
    void processLoadQueue() {
        // Worker thread
        while (!m_loadQueue.empty()) {
            auto request = m_loadQueue.pop();
            
            switch (request.type) {
                case AssetType::Mesh:
                    loadMeshAsync(request.id, request.path);
                    break;
                case AssetType::Texture:
                    loadTextureAsync(request.id, request.path);
                    break;
                // ...
            }
        }
    }
    
    void loadMeshAsync(AssetId id, const std::string& path) {
        // Load on worker thread
        auto mesh = m_gltfLoader.loadMesh(path);
        
        // Queue GPU upload on main thread
        m_gpuUploadQueue.push({id, std::move(mesh)});
    }
    
    ConcurrentQueue<LoadRequest> m_loadQueue;
    ConcurrentQueue<GPUUploadRequest> m_gpuUploadQueue;
};
```

### LOD System

```cpp
class LODManager {
public:
    void selectLOD(const RenderableComponent& renderable, 
                   const glm::vec3& cameraPos) {
        float distance = glm::distance(cameraPos, renderable.position);
        
        // Select LOD based on distance
        int lodLevel = 0;
        for (int i = 0; i < renderable.mesh->lodCount; i++) {
            if (distance > renderable.mesh->lodDistances[i]) {
                lodLevel = i;
            } else {
                break;
            }
        }
        
        renderable.currentLOD = lodLevel;
    }
    
    // LOD transitions with dithering to avoid popping
    void updateLODTransition(RenderableComponent& renderable, float dt) {
        if (renderable.targetLOD != renderable.currentLOD) {
            renderable.lodTransition += dt * LOD_TRANSITION_SPEED;
            if (renderable.lodTransition >= 1.0f) {
                renderable.currentLOD = renderable.targetLOD;
                renderable.lodTransition = 0.0f;
            }
        }
    }
};
```

## Animation System

### Skeletal Animation

```cpp
struct Skeleton {
    std::vector<int> parentIndices;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<std::string> jointNames;
};

struct AnimationClip {
    std::string name;
    float duration;
    
    struct Channel {
        int targetJoint;
        enum class Path { Translation, Rotation, Scale };
        Path path;
        
        std::vector<float> times;
        std::vector<glm::vec4> values;  // vec3 for T/S, quat for R
        
        enum class Interpolation { Step, Linear, CubicSpline };
        Interpolation interpolation;
    };
    
    std::vector<Channel> channels;
};

class AnimationPlayer {
public:
    void update(float dt, const AnimationClip& clip, 
                std::vector<glm::mat4>& jointMatrices) {
        m_time += dt;
        if (m_time > clip.duration) {
            m_time = fmod(m_time, clip.duration);
        }
        
        // Sample each channel
        std::vector<glm::vec3> translations(jointMatrices.size());
        std::vector<glm::quat> rotations(jointMatrices.size());
        std::vector<glm::vec3> scales(jointMatrices.size(), glm::vec3(1.0f));
        
        for (const auto& channel : clip.channels) {
            glm::vec4 value = sampleChannel(channel, m_time);
            
            switch (channel.path) {
                case AnimationClip::Channel::Path::Translation:
                    translations[channel.targetJoint] = glm::vec3(value);
                    break;
                case AnimationClip::Channel::Path::Rotation:
                    rotations[channel.targetJoint] = glm::quat(value.w, value.x, value.y, value.z);
                    break;
                case AnimationClip::Channel::Path::Scale:
                    scales[channel.targetJoint] = glm::vec3(value);
                    break;
            }
        }
        
        // Build joint matrices
        for (size_t i = 0; i < jointMatrices.size(); i++) {
            glm::mat4 local = glm::translate(glm::mat4(1.0f), translations[i])
                            * glm::mat4_cast(rotations[i])
                            * glm::scale(glm::mat4(1.0f), scales[i]);
            
            if (m_skeleton->parentIndices[i] >= 0) {
                jointMatrices[i] = jointMatrices[m_skeleton->parentIndices[i]] * local;
            } else {
                jointMatrices[i] = local;
            }
        }
        
        // Apply inverse bind matrices
        for (size_t i = 0; i < jointMatrices.size(); i++) {
            jointMatrices[i] = jointMatrices[i] * m_skeleton->inverseBindMatrices[i];
        }
    }
    
private:
    glm::vec4 sampleChannel(const AnimationClip::Channel& channel, float time) {
        // Find keyframes
        size_t i = 0;
        for (; i < channel.times.size() - 1; i++) {
            if (time < channel.times[i + 1]) break;
        }
        
        float t = (time - channel.times[i]) / (channel.times[i + 1] - channel.times[i]);
        
        if (channel.interpolation == AnimationClip::Channel::Interpolation::Step) {
            return channel.values[i];
        } else if (channel.interpolation == AnimationClip::Channel::Interpolation::Linear) {
            if (channel.path == AnimationClip::Channel::Path::Rotation) {
                // Spherical interpolation for quaternions
                glm::quat q0(channel.values[i].w, channel.values[i].x, 
                            channel.values[i].y, channel.values[i].z);
                glm::quat q1(channel.values[i+1].w, channel.values[i+1].x,
                            channel.values[i+1].y, channel.values[i+1].z);
                glm::quat result = glm::slerp(q0, q1, t);
                return glm::vec4(result.x, result.y, result.z, result.w);
            } else {
                return glm::mix(channel.values[i], channel.values[i + 1], t);
            }
        }
        
        return channel.values[i];
    }
    
    float m_time = 0.0f;
    const Skeleton* m_skeleton = nullptr;
};
```

## Asset Manifest

### Manifest Format

```json
{
  "version": "1.0",
  "assets": [
    {
      "id": "player_ct",
      "type": "model",
      "path": "models/player/ct_urban.gltf",
      "lods": [
        { "level": 0, "distance": 0, "path": "models/player/ct_urban_lod0.gltf" },
        { "level": 1, "distance": 50, "path": "models/player/ct_urban_lod1.gltf" },
        { "level": 2, "distance": 100, "path": "models/player/ct_urban_lod2.gltf" }
      ],
      "streaming": {
        "priority": "high",
        "preload": true
      }
    },
    {
      "id": "de_dust2",
      "type": "map",
      "path": "maps/de_dust2.bsp",
      "convert": true
    }
  ]
}
```

