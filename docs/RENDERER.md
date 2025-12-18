# Renderer Documentation

## Overview

The renderer implements a modern deferred PBR pipeline using OpenGL 4.5+ core profile. It supports physically-based materials, image-based lighting (IBL), and various post-processing effects.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Render Pipeline                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐                                               │
│  │    Frame     │                                               │
│  │    Begin     │                                               │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐     ┌────────────────────────────────────┐   │
│  │   Shadow     │────▶│  Shadow Map FBO (2048x2048 x CSM)  │   │
│  │    Pass      │     └────────────────────────────────────┘   │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐     ┌────────────────────────────────────┐   │
│  │  G-Buffer    │────▶│  Albedo | Normal | Material | Depth │   │
│  │    Pass      │     └────────────────────────────────────┘   │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐     ┌────────────────────────────────────┐   │
│  │  Lighting    │────▶│       HDR Color Buffer              │   │
│  │    Pass      │     └────────────────────────────────────┘   │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │  Forward     │  (Transparent objects)                        │
│  │    Pass      │                                               │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐     ┌────────────────────────────────────┐   │
│  │    Post      │────▶│  SSAO → Bloom → Tonemap → FXAA     │   │
│  │  Processing  │     └────────────────────────────────────┘   │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │     UI       │  (ImGui + HUD)                                │
│  │    Pass      │                                               │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │    Frame     │                                               │
│  │     End      │                                               │
│  └──────────────┘                                               │
└─────────────────────────────────────────────────────────────────┘
```

## G-Buffer Layout

### Render Targets

| Attachment | Format | Contents |
|------------|--------|----------|
| RT0 | RGBA8 | RGB = Albedo, A = unused |
| RT1 | RG16F | RG = Octahedral encoded normal |
| RT2 | RGBA8 | R = Roughness, G = Metallic, B = AO, A = Flags |
| RT3 | RGB16F | Emissive color (HDR) |
| Depth | D32F | Reversed-Z depth |

### Normal Encoding

Using octahedral encoding for efficient 2-component normals:

```glsl
// Encode world normal to 2 components
vec2 encodeNormal(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return n.xy * 0.5 + 0.5;
}

// Decode back to world normal
vec3 decodeNormal(vec2 enc) {
    enc = enc * 2.0 - 1.0;
    vec3 n = vec3(enc.xy, 1.0 - abs(enc.x) - abs(enc.y));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    }
    return normalize(n);
}
```

### Reversed-Z Depth

For better depth precision:

```cpp
// Projection matrix setup
glm::mat4 createReversedZProjection(float fov, float aspect, float near) {
    float f = 1.0f / tan(fov * 0.5f);
    return glm::mat4(
        f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, near, 0.0f
    );
}

// Depth test setup
glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
glDepthFunc(GL_GEQUAL);  // Reversed
glClearDepth(0.0f);      // Clear to 0 (far)
```

## PBR Material System

### Material Definition

```cpp
struct PBRMaterial {
    // Texture handles (bindless if supported)
    GLuint64 albedoHandle = 0;
    GLuint64 normalHandle = 0;
    GLuint64 metallicRoughnessHandle = 0;
    GLuint64 aoHandle = 0;
    GLuint64 emissiveHandle = 0;
    
    // Factors
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    
    // Alpha
    enum class AlphaMode { Opaque, Mask, Blend };
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    
    // Flags
    bool doubleSided = false;
    bool unlit = false;
};
```

### BRDF Implementation

Cook-Torrance microfacet BRDF:

```glsl
// GGX/Trowbridge-Reitz normal distribution
float D_GGX(float NoH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;
    float denom = NoH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith GGX geometry function
float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float NoV, float NoL, float roughness) {
    return G_SchlickGGX(NoV, roughness) * G_SchlickGGX(NoL, roughness);
}

// Fresnel-Schlick
vec3 F_Schlick(float HoV, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - HoV, 5.0);
}

// Full BRDF
vec3 BRDF(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    
    float NoV = max(dot(N, V), 0.001);
    float NoL = max(dot(N, L), 0.001);
    float NoH = max(dot(N, H), 0.0);
    float HoV = max(dot(H, V), 0.0);
    
    // F0 for dielectrics is 0.04, for metals it's the albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Cook-Torrance specular
    float D = D_GGX(NoH, roughness);
    float G = G_Smith(NoV, NoL, roughness);
    vec3 F = F_Schlick(HoV, F0);
    
    vec3 specular = (D * G * F) / (4.0 * NoV * NoL + 0.001);
    
    // Diffuse (only for dielectrics)
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;
    
    return diffuse + specular;
}
```

## Image-Based Lighting

### Environment Maps

```cpp
struct Environment {
    GLuint skyboxTexture;           // Original HDR cubemap
    GLuint irradianceMap;           // Diffuse irradiance (32x32 faces)
    GLuint prefilteredMap;          // Specular prefiltered (128x128, 5 mips)
    GLuint brdfLUT;                 // 2D BRDF integration LUT (512x512)
};
```

### IBL Sampling

```glsl
// Diffuse IBL
vec3 irradiance = texture(u_irradianceMap, N).rgb;
vec3 diffuseIBL = irradiance * albedo;

// Specular IBL
float lod = roughness * MAX_REFLECTION_LOD;
vec3 prefilteredColor = textureLod(u_prefilteredMap, R, lod).rgb;
vec2 brdf = texture(u_brdfLUT, vec2(NoV, roughness)).rg;
vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

// Combine
vec3 ambient = (kD * diffuseIBL + specularIBL) * ao;
```

### Prefiltering Pipeline

```cpp
void generateEnvironmentMaps(GLuint hdrTexture) {
    // 1. Convert equirectangular to cubemap
    GLuint envCubemap = equirectToCubemap(hdrTexture, 512);
    
    // 2. Generate irradiance map (diffuse convolution)
    GLuint irradianceMap = convolveIrradiance(envCubemap, 32);
    
    // 3. Generate prefiltered specular map
    GLuint prefilteredMap = prefilterSpecular(envCubemap, 128, 5);
    
    // 4. Generate BRDF LUT (can be done once and reused)
    GLuint brdfLUT = generateBRDFLUT(512);
}
```

## Lighting Pass

### Deferred Lighting Shader

```glsl
#version 450 core

// G-buffer inputs
layout(binding = 0) uniform sampler2D u_albedo;
layout(binding = 1) uniform sampler2D u_normal;
layout(binding = 2) uniform sampler2D u_material;
layout(binding = 3) uniform sampler2D u_emissive;
layout(binding = 4) uniform sampler2D u_depth;

// IBL
layout(binding = 5) uniform samplerCube u_irradiance;
layout(binding = 6) uniform samplerCube u_prefiltered;
layout(binding = 7) uniform sampler2D u_brdfLUT;

// Shadow maps
layout(binding = 8) uniform sampler2DArrayShadow u_shadowCascades;

// Lights
struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

layout(std430, binding = 0) readonly buffer LightBuffer {
    PointLight lights[];
};

uniform int u_lightCount;
uniform vec3 u_cameraPos;
uniform mat4 u_invViewProj;

// Output
out vec4 fragColor;

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 worldPos = u_invViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    
    // Sample G-buffer
    vec4 albedoSample = texture(u_albedo, uv);
    vec3 albedo = albedoSample.rgb;
    
    vec3 N = decodeNormal(texture(u_normal, uv).rg);
    
    vec4 material = texture(u_material, uv);
    float roughness = material.r;
    float metallic = material.g;
    float ao = material.b;
    
    vec3 emissive = texture(u_emissive, uv).rgb;
    float depth = texture(u_depth, uv).r;
    
    // Early out for sky
    if (depth == 0.0) {
        fragColor = vec4(textureLod(u_prefiltered, N, 0.0).rgb, 1.0);
        return;
    }
    
    // Reconstruct position
    vec3 worldPos = reconstructWorldPos(uv, depth);
    vec3 V = normalize(u_cameraPos - worldPos);
    
    // Accumulate lighting
    vec3 Lo = vec3(0.0);
    
    // Directional light (sun)
    {
        vec3 L = normalize(-u_sunDirection);
        float shadow = calculateShadow(worldPos);
        vec3 radiance = u_sunColor * u_sunIntensity * shadow;
        Lo += BRDF(N, V, L, albedo, metallic, roughness) * radiance * max(dot(N, L), 0.0);
    }
    
    // Point lights
    for (int i = 0; i < u_lightCount; i++) {
        PointLight light = lights[i];
        
        vec3 L = light.position - worldPos;
        float distance = length(L);
        
        if (distance > light.radius) continue;
        
        L /= distance;
        
        float attenuation = 1.0 / (distance * distance + 1.0);
        attenuation *= smoothstep(light.radius, light.radius * 0.75, distance);
        
        vec3 radiance = light.color * light.intensity * attenuation;
        Lo += BRDF(N, V, L, albedo, metallic, roughness) * radiance * max(dot(N, L), 0.0);
    }
    
    // IBL ambient
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F_SchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 irradiance = texture(u_irradiance, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;
    
    vec3 R = reflect(-V, N);
    float lod = roughness * 4.0;
    vec3 prefilteredColor = textureLod(u_prefiltered, R, lod).rgb;
    vec2 brdf = texture(u_brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
    
    vec3 ambient = (kD * diffuseIBL + specularIBL) * ao;
    
    // Final color
    vec3 color = ambient + Lo + emissive;
    
    fragColor = vec4(color, 1.0);
}
```

## Post-Processing

### Bloom

Two-pass Gaussian blur with threshold:

```glsl
// Downsample with brightness threshold
vec3 color = texture(u_hdrBuffer, uv).rgb;
float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
if (brightness > u_bloomThreshold) {
    fragColor = vec4(color * (brightness - u_bloomThreshold), 1.0);
} else {
    fragColor = vec4(0.0);
}

// Blur passes (horizontal then vertical, repeated for each mip)
// Final: additive blend with original HDR buffer
```

### SSAO

Screen-space ambient occlusion:

```glsl
// Sample kernel in hemisphere
const int KERNEL_SIZE = 64;
uniform vec3 u_kernel[KERNEL_SIZE];
uniform sampler2D u_noise;

float ssao(vec2 uv, vec3 fragPos, vec3 normal) {
    vec3 randomVec = texture(u_noise, uv * u_noiseScale).xyz * 2.0 - 1.0;
    
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    for (int i = 0; i < KERNEL_SIZE; i++) {
        vec3 samplePos = fragPos + TBN * u_kernel[i] * u_radius;
        vec4 offset = u_projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        
        float sampleDepth = linearizeDepth(texture(u_depth, offset.xy).r);
        float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + u_bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    return 1.0 - (occlusion / KERNEL_SIZE);
}
```

### Tone Mapping

ACES filmic tone mapping:

```glsl
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Apply in final pass
vec3 hdrColor = texture(u_hdrBuffer, uv).rgb;
vec3 mapped = ACESFilm(hdrColor * u_exposure);
fragColor = vec4(pow(mapped, vec3(1.0 / 2.2)), 1.0);  // Gamma correction
```

## Culling

### Frustum Culling

```cpp
struct Frustum {
    glm::vec4 planes[6];  // left, right, bottom, top, near, far
    
    void extractFromMatrix(const glm::mat4& viewProj) {
        // Left
        planes[0] = glm::vec4(
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        );
        // ... other planes
        
        // Normalize
        for (auto& p : planes) {
            p /= glm::length(glm::vec3(p));
        }
    }
    
    bool testAABB(const AABB& aabb) const {
        for (const auto& plane : planes) {
            glm::vec3 positive = aabb.min;
            if (plane.x >= 0) positive.x = aabb.max.x;
            if (plane.y >= 0) positive.y = aabb.max.y;
            if (plane.z >= 0) positive.z = aabb.max.z;
            
            if (glm::dot(glm::vec3(plane), positive) + plane.w < 0) {
                return false;  // Outside frustum
            }
        }
        return true;
    }
};
```

### GPU Culling (Compute)

```glsl
#version 450 core
layout(local_size_x = 64) in;

struct DrawCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    uint baseVertex;
    uint baseInstance;
};

struct ObjectData {
    mat4 transform;
    vec4 boundingSphere;  // xyz = center, w = radius
    uint meshIndex;
    uint materialIndex;
    uint padding[2];
};

layout(std430, binding = 0) readonly buffer Objects {
    ObjectData objects[];
};

layout(std430, binding = 1) writeonly buffer DrawCommands {
    DrawCommand commands[];
};

layout(std430, binding = 2) buffer VisibleCount {
    uint visibleCount;
};

uniform mat4 u_viewProj;
uniform vec4 u_frustumPlanes[6];

bool isVisible(vec4 boundingSphere) {
    vec3 center = (u_viewProj * vec4(boundingSphere.xyz, 1.0)).xyz;
    float radius = boundingSphere.w;
    
    for (int i = 0; i < 6; i++) {
        if (dot(u_frustumPlanes[i].xyz, boundingSphere.xyz) + u_frustumPlanes[i].w < -radius) {
            return false;
        }
    }
    return true;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_objectCount) return;
    
    ObjectData obj = objects[idx];
    
    if (isVisible(obj.boundingSphere)) {
        uint slot = atomicAdd(visibleCount, 1);
        commands[slot] = /* build draw command */;
    }
}
```

## Shader Compilation

### Shader Program Management

```cpp
class ShaderManager {
public:
    GLuint loadShader(const std::string& vertPath, const std::string& fragPath) {
        std::string vertSrc = loadFile(vertPath);
        std::string fragSrc = loadFile(fragPath);
        
        // Preprocess includes
        vertSrc = preprocessIncludes(vertSrc);
        fragSrc = preprocessIncludes(fragSrc);
        
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        
        // Check errors...
        
        glDeleteShader(vs);
        glDeleteShader(fs);
        
        return program;
    }
    
private:
    std::string preprocessIncludes(const std::string& src) {
        std::string result = src;
        std::regex includeRegex(R"(#include\s+[<"]([^>"]+)[>"])");
        
        std::smatch match;
        while (std::regex_search(result, match, includeRegex)) {
            std::string includePath = match[1].str();
            std::string includeContent = loadFile("shaders/common/" + includePath);
            result = match.prefix().str() + includeContent + match.suffix().str();
        }
        
        return result;
    }
};
```

## Debug Visualization

### Wireframe Mode

```cpp
void renderWireframe(const Scene& scene) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    
    // Render with simple color shader
    m_wireframeShader.bind();
    for (auto& object : scene.objects) {
        m_wireframeShader.setUniform("u_color", glm::vec3(0, 1, 0));
        object.draw();
    }
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
}
```

### G-Buffer Visualization

```cpp
void debugGBuffer(GBufferTarget target) {
    m_debugQuadShader.bind();
    
    switch (target) {
        case GBufferTarget::Albedo:
            glBindTextureUnit(0, m_gbuffer.albedo);
            break;
        case GBufferTarget::Normal:
            glBindTextureUnit(0, m_gbuffer.normal);
            m_debugQuadShader.setUniform("u_decodeNormal", true);
            break;
        case GBufferTarget::Depth:
            glBindTextureUnit(0, m_gbuffer.depth);
            m_debugQuadShader.setUniform("u_linearizeDepth", true);
            break;
        // ...
    }
    
    m_fullscreenQuad.draw();
}
```

