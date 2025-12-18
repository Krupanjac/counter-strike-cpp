#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"
#include <vector>

namespace cscpp::renderer {

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
};

class GLMesh {
public:
    GLMesh() = default;
    ~GLMesh();
    
    // Delete copy constructor/assignment (OpenGL objects can't be copied)
    GLMesh(const GLMesh&) = delete;
    GLMesh& operator=(const GLMesh&) = delete;
    
    // Move constructor
    GLMesh(GLMesh&& other) noexcept
        : m_vao(other.m_vao)
        , m_vbo(other.m_vbo)
        , m_ebo(other.m_ebo)
        , m_indexCount(other.m_indexCount)
    {
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
        other.m_indexCount = 0;
    }
    
    // Move assignment
    GLMesh& operator=(GLMesh&& other) noexcept {
        if (this != &other) {
            destroy();
            m_vao = other.m_vao;
            m_vbo = other.m_vbo;
            m_ebo = other.m_ebo;
            m_indexCount = other.m_indexCount;
            other.m_vao = 0;
            other.m_vbo = 0;
            other.m_ebo = 0;
            other.m_indexCount = 0;
        }
        return *this;
    }
    
    /// Create mesh from vertex data
    void create(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);
    
    /// Draw the mesh
    void draw() const;
    
    /// Destroy the mesh
    void destroy();
    
    bool isValid() const { return m_vao != 0; }
    
private:
    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_ebo = 0;
    u32 m_indexCount = 0;
};

} // namespace cscpp::renderer

