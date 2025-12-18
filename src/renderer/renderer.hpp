#pragma once

/**
 * @file renderer.hpp
 * @brief Modern OpenGL deferred PBR renderer
 */

#include "core/types.hpp"
#include "core/math/math.hpp"

namespace cscpp::renderer {

/**
 * @brief Renderer configuration
 */
struct RendererConfig {
    i32 width = 1920;
    i32 height = 1080;
    bool vsync = true;
    i32 msaaSamples = 0;
    bool enableHDR = true;
    bool enableBloom = true;
    bool enableSSAO = false;
    i32 shadowMapSize = 2048;
    i32 shadowCascades = 4;
};

/**
 * @brief G-Buffer render target indices
 */
enum class GBufferTarget {
    Albedo,
    Normal,
    Material,  // Roughness, Metallic, AO
    Emissive,
    Depth
};

/**
 * @brief Main renderer class
 * 
 * Implements a deferred PBR rendering pipeline:
 * 1. Shadow pass
 * 2. G-Buffer pass
 * 3. Lighting pass (deferred)
 * 4. Forward pass (transparent)
 * 5. Post-processing
 * 6. UI pass
 */
class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    
    /// Initialize the renderer
    Result<void> initialize(const RendererConfig& config);
    
    /// Shutdown the renderer
    void shutdown();
    
    /// Begin a new frame
    void beginFrame();
    
    /// End frame and present
    void endFrame();
    
    /// Resize the render targets
    void resize(i32 width, i32 height);
    
    // ========================================================================
    // Render Passes
    // ========================================================================
    
    /// Shadow map pass
    void beginShadowPass();
    void endShadowPass();
    
    /// G-Buffer pass
    void beginGBufferPass();
    void endGBufferPass();
    
    /// Deferred lighting pass
    void beginLightingPass();
    void endLightingPass();
    
    /// Forward pass (transparent objects)
    void beginForwardPass();
    void endForwardPass();
    
    /// Post-processing
    void applyPostProcessing();
    
    /// UI pass (ImGui)
    void beginUIPass();
    void endUIPass();
    
    // ========================================================================
    // Debug
    // ========================================================================
    
    /// Show G-Buffer debug visualization
    void debugGBuffer(GBufferTarget target);
    
    /// Enable wireframe mode
    void setWireframe(bool enabled);
    
    /// Get frame statistics
    struct FrameStats {
        f32 gpuTime;
        i32 drawCalls;
        i32 triangles;
        i32 textureBinds;
    };
    FrameStats getFrameStats() const;
    
private:
    void createGBuffer();
    void createPostProcessTargets();
    void loadShaders();
    
    RendererConfig m_config;
    
    // G-Buffer textures
    u32 m_gBufferFBO = 0;
    u32 m_albedoTexture = 0;
    u32 m_normalTexture = 0;
    u32 m_materialTexture = 0;
    u32 m_emissiveTexture = 0;
    u32 m_depthTexture = 0;
    
    // HDR lighting buffer
    u32 m_hdrFBO = 0;
    u32 m_hdrColorTexture = 0;
    
    // Post-process targets
    u32 m_bloomFBO = 0;
    u32 m_bloomTextures[2] = {0, 0};
    
    // Shader programs
    u32 m_gBufferShader = 0;
    u32 m_lightingShader = 0;
    u32 m_forwardShader = 0;
    u32 m_bloomShader = 0;
    u32 m_tonemapShader = 0;
    
    // Fullscreen quad VAO
    u32 m_quadVAO = 0;
    
    bool m_wireframe = false;
    FrameStats m_stats;
};

} // namespace cscpp::renderer

