#include "assets/gltf/simple_gltf_loader.hpp"
#include "core/logging/logger.hpp"
#include <fstream>
#include <filesystem>
#include <glad/glad.h>

#if defined(TINYGLTF_FOUND) || __has_include(<tiny_gltf.h>)
// Define stb_image implementation before tinygltf includes it
// tinygltf will include stb_image.h itself, so we just need to define the implementation macro
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#define HAS_TINYGLTF 1
#else
#define HAS_TINYGLTF 0
#endif

namespace cscpp::assets {

Result<SimpleModel> SimpleGLTFLoader::load(const std::string& path) {
    LOG_INFO("Loading glTF model: {}", path);
    
    // Try multiple paths (relative to executable directory)
    // Assets are copied to build/bin/Release/assets by CMake
    std::vector<std::string> tryPaths = {
        path,  // Original path as-is
        "../" + path,
        "../../" + path,
        "../../../" + path,
        // Specific paths for AK-47
        "assets/weapons/ak-47/scene.gltf",
        "../assets/weapons/ak-47/scene.gltf",
        "../../assets/weapons/ak-47/scene.gltf",
        "../../../assets/weapons/ak-47/scene.gltf",
        // Also try from source directory (for development)
        "../../../../assets/weapons/ak-47/scene.gltf",
        "../../../../../assets/weapons/ak-47/scene.gltf",
    };
    
    std::string foundPath;
    bool found = false;
    for (const auto& tryPath : tryPaths) {
        std::ifstream file(tryPath);
        if (file.is_open()) {
            foundPath = tryPath;
            found = true;
            file.close();
            break;
        }
    }
    
    if (!found) {
        LOG_WARN("glTF file not found in any location, creating test mesh");
        return createTestWeaponMesh();
    }
    
    LOG_INFO("Found glTF file at: {}", foundPath);
    
#if HAS_TINYGLTF
    return loadGLTF(foundPath);
#else
    LOG_WARN("tinygltf not found, using test mesh");
    return createTestWeaponMesh();
#endif
}

#if HAS_TINYGLTF
// File system callbacks for tinygltf
static bool FileExists(const std::string& filepath, void* user_data) {
    (void)user_data;
    std::ifstream file(filepath);
    return file.good();
}

static bool ReadWholeFile(std::vector<unsigned char>* out, std::string* err,
                          const std::string& filepath, void* user_data) {
    (void)user_data;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        if (err) {
            *err = "Failed to open file: " + filepath;
        }
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    out->resize(size);
    file.read(reinterpret_cast<char*>(out->data()), size);
    
    return true;
}

static bool WriteWholeFile(std::string* err, const std::string& filepath,
                           const std::vector<unsigned char>& contents, void* user_data) {
    (void)user_data;
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        if (err) {
            *err = "Failed to open file for writing: " + filepath;
        }
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(contents.data()), contents.size());
    return true;
}

static bool GetFileSizeInBytes(size_t* filesize_out, std::string* err,
                               const std::string& filepath, void* user_data) {
    (void)user_data;
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        if (err) {
            *err = "Failed to open file: " + filepath;
        }
        return false;
    }
    
    *filesize_out = file.tellg();
    return true;
}

static std::string ExpandFilePath(const std::string& filepath, void* user_data) {
    (void)user_data;
    return filepath;
}

Result<SimpleModel> SimpleGLTFLoader::loadGLTF(const std::string& path) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    
    // Set up file system callbacks
    tinygltf::FsCallbacks fsCallbacks;
    fsCallbacks.FileExists = &FileExists;
    fsCallbacks.ReadWholeFile = &ReadWholeFile;
    fsCallbacks.WriteWholeFile = &WriteWholeFile;
    fsCallbacks.ExpandFilePath = &ExpandFilePath;
    fsCallbacks.GetFileSizeInBytes = &GetFileSizeInBytes;
    fsCallbacks.user_data = nullptr;
    
    loader.SetFsCallbacks(fsCallbacks);
    
    bool success = false;
    std::filesystem::path filePath(path);
    if (filePath.extension() == ".glb") {
        success = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    } else {
        success = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    }
    
    if (!warn.empty()) {
        LOG_WARN("glTF warning: {}", warn);
    }
    
    if (!err.empty() || !success) {
        LOG_ERROR("glTF error: {}", err);
        return std::unexpected(Error{"Failed to load glTF: " + err});
    }
    
    SimpleModel result;
    
    // Process all meshes in the model
    std::vector<renderer::Vertex> allVertices;
    std::vector<u32> allIndices;
    u32 indexOffset = 0;
    
    // Get the base directory for loading textures and buffers
    std::filesystem::path baseDir = std::filesystem::path(path).parent_path();
    
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            // Extract positions
            std::vector<Vec3> positions;
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                int posAccessorIdx = primitive.attributes.at("POSITION");
                const auto& accessor = model.accessors[posAccessorIdx];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                
                // Calculate data offset
                size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;
                if (dataOffset + accessor.count * 3 * sizeof(float) > buffer.data.size()) {
                    LOG_WARN("Position data out of bounds, skipping");
                } else {
                    const float* posData = reinterpret_cast<const float*>(&buffer.data[dataOffset]);
                    size_t posCount = accessor.count;
                    
                    for (size_t i = 0; i < posCount; ++i) {
                        positions.push_back(Vec3(
                            posData[i * 3 + 0],
                            posData[i * 3 + 1],
                            posData[i * 3 + 2]
                        ));
                    }
                }
            }
            
            // Extract normals
            std::vector<Vec3> normals;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                int normAccessorIdx = primitive.attributes.at("NORMAL");
                const auto& accessor = model.accessors[normAccessorIdx];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                
                size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;
                if (dataOffset + accessor.count * 3 * sizeof(float) > buffer.data.size()) {
                    LOG_WARN("Normal data out of bounds, skipping");
                } else {
                    const float* normData = reinterpret_cast<const float*>(&buffer.data[dataOffset]);
                    size_t normCount = accessor.count;
                    
                    for (size_t i = 0; i < normCount; ++i) {
                        normals.push_back(Vec3(
                            normData[i * 3 + 0],
                            normData[i * 3 + 1],
                            normData[i * 3 + 2]
                        ));
                    }
                }
            }
            
            // Extract texture coordinates
            std::vector<Vec2> texCoords;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                int texAccessorIdx = primitive.attributes.at("TEXCOORD_0");
                const auto& accessor = model.accessors[texAccessorIdx];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                
                size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;
                if (dataOffset + accessor.count * 2 * sizeof(float) > buffer.data.size()) {
                    LOG_WARN("TexCoord data out of bounds, skipping");
                } else {
                    const float* texData = reinterpret_cast<const float*>(&buffer.data[dataOffset]);
                    size_t texCount = accessor.count;
                    
                    for (size_t i = 0; i < texCount; ++i) {
                        texCoords.push_back(Vec2(
                            texData[i * 2 + 0],
                            texData[i * 2 + 1]
                        ));
                    }
                }
            }
            
            // Extract indices
            std::vector<u32> indices;
            if (primitive.indices >= 0) {
                const auto& accessor = model.accessors[primitive.indices];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                
                size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;
                size_t indexSize = (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) ? sizeof(u16) : sizeof(u32);
                
                if (dataOffset + accessor.count * indexSize > buffer.data.size()) {
                    LOG_WARN("Index data out of bounds, skipping");
                } else {
                    const unsigned char* indexData = &buffer.data[dataOffset];
                    size_t indexCount = accessor.count;
                    
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const u16* indices16 = reinterpret_cast<const u16*>(indexData);
                        for (size_t i = 0; i < indexCount; ++i) {
                            indices.push_back(static_cast<u32>(indices16[i]) + indexOffset);
                        }
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const u32* indices32 = reinterpret_cast<const u32*>(indexData);
                        for (size_t i = 0; i < indexCount; ++i) {
                            indices.push_back(indices32[i] + indexOffset);
                        }
                    }
                }
            }
            
            // Create vertices (combine positions, normals, texCoords)
            size_t vertexCount = positions.size();
            for (size_t i = 0; i < vertexCount; ++i) {
                renderer::Vertex vertex;
                vertex.position = positions[i];
                vertex.normal = (i < normals.size()) ? normals[i] : Vec3(0.0f, 1.0f, 0.0f);
                vertex.texCoord = (i < texCoords.size()) ? texCoords[i] : Vec2(0.0f, 0.0f);
                allVertices.push_back(vertex);
            }
            
            // Add indices
            for (u32 idx : indices) {
                allIndices.push_back(idx);
            }
            
            indexOffset += static_cast<u32>(vertexCount);
        }
    }
    
    if (allVertices.empty() || allIndices.empty()) {
        LOG_WARN("No vertex data found in glTF, using test mesh");
        return createTestWeaponMesh();
    }
    
    // Load textures from glTF
    // Try to find a material with a base color texture
    u32 textureID = 0;
    for (const auto& material : model.materials) {
        if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            if (textureIndex >= 0 && textureIndex < static_cast<int>(model.textures.size())) {
                const auto& texture = model.textures[textureIndex];
                if (texture.source >= 0 && texture.source < static_cast<int>(model.images.size())) {
                    const auto& image = model.images[texture.source];
                    
                    // Load image data
                    std::vector<u8> imageData;
                    int width = 0, height = 0, channels = 0;
                    
                    if (!image.image.empty() && image.width > 0 && image.height > 0) {
                        // Image data is already decoded by tinygltf
                        imageData = image.image;
                        width = image.width;
                        height = image.height;
                        channels = image.component; // 1=gray, 2=gray+alpha, 3=RGB, 4=RGBA
                    } else if (!image.uri.empty()) {
                        // Image is in external file - try to load it
                        std::filesystem::path imagePath = baseDir / image.uri;
                        std::string imagePathStr = imagePath.string();
                        
                        // Try multiple paths
                        std::vector<std::string> tryPaths = {
                            imagePathStr,
                            image.uri,
                            baseDir.string() + "/" + image.uri,
                            std::filesystem::path(path).parent_path().string() + "/" + image.uri
                        };
                        
                        bool loaded = false;
                        for (const auto& tryPath : tryPaths) {
                            u8* data = stbi_load(tryPath.c_str(), &width, &height, &channels, 0);
                            if (data) {
                                imageData.assign(data, data + width * height * channels);
                                stbi_image_free(data);
                                loaded = true;
                                LOG_INFO("Loaded texture from: {}", tryPath);
                                break;
                            }
                        }
                        
                        if (!loaded) {
                            LOG_WARN("Failed to load texture image from any path. Tried: {}", image.uri);
                            continue;
                        }
                    } else {
                        LOG_WARN("Image has no data and no URI");
                        continue;
                    }
                    
                    if (!imageData.empty() && width > 0 && height > 0) {
                        // Create OpenGL texture
                        glGenTextures(1, &textureID);
                        if (textureID != 0) {
                            glBindTexture(GL_TEXTURE_2D, textureID);
                            
                            // Determine format
                            GLenum format = GL_RGB;
                            if (channels == 4) {
                                format = GL_RGBA;
                            } else if (channels == 1) {
                                format = GL_RED;
                            }
                            
                            // Upload texture data
                            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, imageData.data());
                            
                            // Set texture parameters
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                            
                            glBindTexture(GL_TEXTURE_2D, 0);
                            
                            LOG_INFO("Loaded texture from glTF: {}x{} ({} channels, ID: {})", width, height, channels, textureID);
                            break; // Use first texture found
                        }
                    }
                }
            }
        }
    }
    
    result.mesh.create(allVertices, allIndices);
    result.textureID = textureID;
    result.loaded = true;
    
    LOG_INFO("Loaded glTF model: {} vertices, {} indices, texture ID: {}", 
             allVertices.size(), allIndices.size(), textureID);
    
    return result;
}
#endif

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

