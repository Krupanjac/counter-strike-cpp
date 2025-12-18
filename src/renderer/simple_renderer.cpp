#include "renderer/simple_renderer.hpp"
#include "core/logging/logger.hpp"
#include <glad/glad.h>
#include <fstream>

namespace cscpp::renderer {

Result<void> SimpleRenderer::initialize() {
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Temporarily disable face culling to debug
    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    // glFrontFace(GL_CCW);
    
    // Set default clear color
    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    
    // Try multiple paths for shaders
    std::vector<std::pair<std::string, std::string>> shaderPaths = {
        {"assets/shaders/basic.vert", "assets/shaders/basic.frag"},
        {"../assets/shaders/basic.vert", "../assets/shaders/basic.frag"},
        {"../../assets/shaders/basic.vert", "../../assets/shaders/basic.frag"},
    };
    
    Result<void> result = std::unexpected(Error{"No shader files found"});
    for (const auto& [vert, frag] : shaderPaths) {
        result = m_basicShader.loadFromFiles(vert, frag);
        if (result) {
            LOG_INFO("Loaded shaders from: {}", vert);
            break;
        }
    }
    
    if (!result) {
        LOG_ERROR("Failed to load basic shader: {}", result.error().message);
        return result;
    }
    
    LOG_INFO("Simple renderer initialized");
    return {};
}

void SimpleRenderer::setViewport(i32 width, i32 height) {
    m_viewportWidth = width;
    m_viewportHeight = height;
    glViewport(0, 0, width, height);
}

void SimpleRenderer::setCamera(const Mat4& view, const Mat4& projection) {
    m_view = view;
    m_projection = projection;
}

void SimpleRenderer::clear(const Vec3& color) {
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void SimpleRenderer::drawMesh(const GLMesh& mesh, const Mat4& model, const Vec3& color) {
    if (!mesh.isValid()) {
        static bool logged = false;
        if (!logged) {
            LOG_WARN("Attempted to draw invalid mesh");
            logged = true;
        }
        return;
    }
    
    if (!m_basicShader.isValid()) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("Shader is not valid!");
            logged = true;
        }
        return;
    }
    
    // Clear any previous errors at the start
    GLenum prevErr = GL_NO_ERROR;
    while ((prevErr = glGetError()) != GL_NO_ERROR) {
        // Clear all previous errors
    }
    
    // Bind shader first
    m_basicShader.bind();
    
    // Check for errors after binding
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("OpenGL error after shader bind: 0x{:X}", static_cast<u32>(err));
            logged = true;
        }
    }
    
    // Set uniforms
    m_basicShader.setUniform("uModel", model);
    m_basicShader.setUniform("uView", m_view);
    m_basicShader.setUniform("uProjection", m_projection);
    m_basicShader.setUniform("uColor", color);
    m_basicShader.setUniform("uUseTexture", false);  // Will be set to true when texture is bound
    
    // Check for errors after setting uniforms
    err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("OpenGL error after setting uniforms: 0x{:X}", static_cast<u32>(err));
            logged = true;
        }
    }
    
    // Ensure shader is still bound before drawing (in case getUniformLocation changed it)
    m_basicShader.bind();
    
    // Verify shader is actually bound
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    if (currentProgram != static_cast<GLint>(m_basicShader.getProgram())) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("Shader program not bound before draw! Expected: {}, Got: {}", 
                     m_basicShader.getProgram(), currentProgram);
            logged = true;
        }
        // Force bind it
        glUseProgram(m_basicShader.getProgram());
    }
    
    // Verify vertex attributes are enabled (they should be part of VAO state)
    // But let's check if the shader program is valid and linked
    GLint linkStatus = 0;
    glGetProgramiv(m_basicShader.getProgram(), GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        static bool logged = false;
        if (!logged) {
            char infoLog[512];
            glGetProgramInfoLog(m_basicShader.getProgram(), 512, nullptr, infoLog);
            LOG_ERROR("Shader program not properly linked: {}", infoLog);
            logged = true;
        }
        return;
    }
    
    // Draw the mesh (VAO will be bound by mesh.draw())
    // Check VAO before drawing
    GLint boundVAOBefore = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAOBefore);
    
    // Verify mesh is valid before drawing
    if (!mesh.isValid()) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("Attempting to draw invalid mesh!");
            logged = true;
        }
        return;
    }
    
    mesh.draw();
    
    // Check VAO after drawing (should still be bound)
    GLint boundVAOAfter = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAOAfter);
    
    // Check for errors after drawing
    err = glGetError();
    if (err != GL_NO_ERROR) {
        static u32 errorCount = 0;
        if (errorCount < 5) {
            LOG_ERROR("OpenGL error after mesh.draw(): 0x{:X} (program: {}, VAO before: {}, VAO after: {})", 
                     static_cast<u32>(err), currentProgram, boundVAOBefore, boundVAOAfter);
            errorCount++;
        }
    }
    
    // Unbind VAO after error check
    glBindVertexArray(0);
}

void SimpleRenderer::drawMeshWithTexture(const GLMesh& mesh, const Mat4& model, u32 textureID, const Vec3& color) {
    if (!mesh.isValid() || textureID == 0) {
        // Fall back to regular draw if invalid
        static bool logged = false;
        if (!logged) {
            LOG_WARN("drawMeshWithTexture called with invalid mesh or texture ID {}", textureID);
            logged = true;
        }
        drawMesh(mesh, model, color);
        return;
    }
    
    if (!m_basicShader.isValid()) {
        return;
    }
    
    // Clear any previous errors
    while (glGetError() != GL_NO_ERROR) {}
    
    // Bind shader first
    m_basicShader.bind();
    
    // Set uniforms
    m_basicShader.setUniform("uModel", model);
    m_basicShader.setUniform("uView", m_view);
    m_basicShader.setUniform("uProjection", m_projection);
    m_basicShader.setUniform("uColor", color);
    m_basicShader.setUniform("uUseTexture", true);
    
    // Bind texture to texture unit 0 BEFORE setting the uniform
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Verify texture is actually bound
    GLint boundTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    if (boundTexture != static_cast<GLint>(textureID)) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("Texture binding failed! Expected: {}, Got: {}", textureID, boundTexture);
            logged = true;
        }
    }
    
    // Set the sampler uniform to use texture unit 0
    // This must be done AFTER binding the texture
    m_basicShader.setUniform("uTexture", 0);
    
    // Debug: log texture binding on first call
    static bool firstTextureBind = true;
    if (firstTextureBind) {
        LOG_INFO("Bound texture ID {} to texture unit 0, verified bound: {}", textureID, boundTexture);
        firstTextureBind = false;
    }
    
    // Check for OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("OpenGL error after binding texture {}: 0x{:X}", textureID, static_cast<u32>(err));
            logged = true;
        }
    }
    
    // Draw mesh
    mesh.draw();
    
    // Check for errors after drawing
    err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("OpenGL error after drawing with texture {}: 0x{:X}", textureID, static_cast<u32>(err));
            logged = true;
        }
    }
    
    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

} // namespace cscpp::renderer

