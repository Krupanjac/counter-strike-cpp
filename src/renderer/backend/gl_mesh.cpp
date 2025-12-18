#include "renderer/backend/gl_mesh.hpp"
#include "core/logging/logger.hpp"
#include <glad/glad.h>
#include <cstddef>

namespace cscpp::renderer {

GLMesh::~GLMesh() {
    destroy();
}

void GLMesh::create(const std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    destroy();
    
    if (vertices.empty() || indices.empty()) {
        return;
    }
    
    m_indexCount = static_cast<u32>(indices.size());
    
    // Generate buffers
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    
    // Check for errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOG_ERROR("Error generating VAO/VBO/EBO: 0x{:X}", static_cast<u32>(err));
        while (glGetError() != GL_NO_ERROR) {}
        destroy();
        return;
    }
    
    if (m_vao == 0) {
        LOG_ERROR("glGenVertexArrays returned 0 - VAO creation failed!");
        destroy();
        return;
    }
    
    LOG_INFO("Created VAO: {}, VBO: {}, EBO: {}, vertices: {}, indices: {}", 
             m_vao, m_vbo, m_ebo, vertices.size(), indices.size());
    
    // Bind VAO
    glBindVertexArray(m_vao);
    
    // Verify VAO is bound
    GLint boundVAO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAO);
    if (boundVAO != static_cast<GLint>(m_vao)) {
        LOG_ERROR("VAO binding failed! Expected: {}, Got: {}", m_vao, boundVAO);
        destroy();
        return;
    }
    
    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    
    // Index buffer (must be bound to VAO)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    
    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    
    // TexCoord attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    
    // Check for errors
    err = glGetError();
    if (err != GL_NO_ERROR) {
        // Log but continue
    }
    
    // Unbind VAO (state is saved in VAO)
    glBindVertexArray(0);
}

void GLMesh::draw() const {
    if (m_vao == 0 || m_indexCount == 0) {
        static bool logged = false;
        if (!logged) {
            LOG_WARN("Cannot draw mesh - VAO: {}, indexCount: {}", m_vao, m_indexCount);
            logged = true;
        }
        return;
    }
    
    // Clear any previous errors
    while (glGetError() != GL_NO_ERROR) {}
    
    // Bind VAO
    glBindVertexArray(m_vao);
    
    // Verify VAO is actually bound
    GLint boundVAO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVAO);
    if (boundVAO != static_cast<GLint>(m_vao)) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("VAO binding failed in draw! Expected: {}, Got: {}", m_vao, boundVAO);
            logged = true;
        }
        return;
    }
    
    // Draw
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    
    // Check for immediate errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool logged = false;
        if (!logged) {
            LOG_ERROR("OpenGL error immediately after glDrawElements: 0x{:X} (VAO: {}, indices: {})", 
                     static_cast<u32>(err), m_vao, m_indexCount);
            logged = true;
        }
    }
    
    // VAO remains bound for caller to check errors
}

void GLMesh::destroy() {
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_indexCount = 0;
}

} // namespace cscpp::renderer

