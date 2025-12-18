#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"
#include "renderer/backend/gl_shader.hpp"
#include "renderer/backend/gl_mesh.hpp"
#include <memory>
#include <vector>

namespace cscpp::renderer {

class SimpleRenderer {
public:
    SimpleRenderer() = default;
    ~SimpleRenderer() = default;
    
    /// Initialize the renderer
    Result<void> initialize();
    
    /// Set viewport size
    void setViewport(i32 width, i32 height);
    
    /// Set camera matrices
    void setCamera(const Mat4& view, const Mat4& projection);
    
    /// Clear the screen
    void clear(const Vec3& color = Vec3(0.1f, 0.1f, 0.15f));
    
    /// Draw a mesh with a model matrix
    void drawMesh(const GLMesh& mesh, const Mat4& model, const Vec3& color = Vec3(1.0f));
    
    /// Draw a mesh with a texture
    void drawMeshWithTexture(const GLMesh& mesh, const Mat4& model, u32 textureID, const Vec3& color = Vec3(1.0f));
    
    /// Get the basic shader
    GLShader& getShader() { return m_basicShader; }
    
private:
    GLShader m_basicShader;
    Mat4 m_view;
    Mat4 m_projection;
    i32 m_viewportWidth = 1920;
    i32 m_viewportHeight = 1080;
};

} // namespace cscpp::renderer

