#include "renderer/backend/gl_shader.hpp"
#include "core/logging/logger.hpp"
#include "core/math/math.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace cscpp::renderer {

GLShader::~GLShader() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
}

Result<void> GLShader::loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    u32 vertexShader = compileShader(GL_VERTEX_SHADER, vertexSrc);
    if (vertexShader == 0) {
        return std::unexpected(Error{"Failed to compile vertex shader"});
    }
    
    u32 fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return std::unexpected(Error{"Failed to compile fragment shader"});
    }
    
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);
    
    // Check linking
    i32 success;
    char infoLog[512];
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(m_program, 512, nullptr, infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(m_program);
        m_program = 0;
        return std::unexpected(Error{std::string("Shader linking failed: ") + infoLog});
    }
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return {};
}

Result<void> GLShader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    // Read vertex shader
    std::ifstream vFile(vertexPath);
    if (!vFile.is_open()) {
        return std::unexpected(Error{"Failed to open vertex shader file: " + vertexPath});
    }
    std::stringstream vStream;
    vStream << vFile.rdbuf();
    std::string vertexSrc = vStream.str();
    vFile.close();
    
    // Read fragment shader
    std::ifstream fFile(fragmentPath);
    if (!fFile.is_open()) {
        return std::unexpected(Error{"Failed to open fragment shader file: " + fragmentPath});
    }
    std::stringstream fStream;
    fStream << fFile.rdbuf();
    std::string fragmentSrc = fStream.str();
    fFile.close();
    
    return loadFromSource(vertexSrc, fragmentSrc);
}

void GLShader::bind() const {
    if (m_program != 0) {
        glUseProgram(m_program);
    }
}

void GLShader::unbind() {
    glUseProgram(0);
}

u32 GLShader::compileShader(u32 type, const std::string& source) {
    u32 shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    i32 success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOG_ERROR("Shader compilation failed: {}", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

i32 GLShader::getUniformLocation(const std::string& name) {
    if (m_program == 0) {
        return -1;
    }
    
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) {
        return it->second;
    }
    
    // Get uniform location - program doesn't need to be bound for this
    // but we'll ensure our program is bound to be safe
    GLint currentProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    
    // Only bind if not already bound
    if (currentProgram != static_cast<GLint>(m_program)) {
        glUseProgram(m_program);
    }
    
    i32 location = glGetUniformLocation(m_program, name.c_str());
    m_uniformCache[name] = location;
    
    // Restore previous program only if it was different
    if (currentProgram != static_cast<GLint>(m_program)) {
        glUseProgram(currentProgram);
    }
    
    // Log warning if uniform not found (but only once per uniform)
    if (location == -1) {
        static std::unordered_set<std::string> warnedUniforms;
        if (warnedUniforms.find(name) == warnedUniforms.end()) {
            LOG_WARN("Uniform '{}' not found in shader (may be optimized out)", name);
            warnedUniforms.insert(name);
        }
    }
    
    return location;
}

void GLShader::setUniform(const std::string& name, f32 value) {
    i32 location = getUniformLocation(name);
    if (location >= 0) {
        glUniform1f(location, value);
    }
}

void GLShader::setUniform(const std::string& name, i32 value) {
    i32 location = getUniformLocation(name);
    if (location >= 0) {
        glUniform1i(location, value);
    }
}

void GLShader::setUniform(const std::string& name, bool value) {
    // GLSL bool uniforms are set as integers (0 or 1)
    setUniform(name, value ? 1 : 0);
}

void GLShader::setUniform(const std::string& name, const Vec2& value) {
    i32 location = getUniformLocation(name);
    if (location >= 0) {
        glUniform2f(location, value.x, value.y);
    }
}

void GLShader::setUniform(const std::string& name, const Vec3& value) {
    i32 location = getUniformLocation(name);
    if (location >= 0) {
        glUniform3f(location, value.x, value.y, value.z);
    }
}

void GLShader::setUniform(const std::string& name, const Vec4& value) {
    i32 location = getUniformLocation(name);
    if (location >= 0) {
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }
}

void GLShader::setUniform(const std::string& name, const Mat4& value) {
    i32 location = getUniformLocation(name);
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }
}

} // namespace cscpp::renderer

