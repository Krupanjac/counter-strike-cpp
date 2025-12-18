#pragma once

#include "core/types.hpp"
#include "core/math/math.hpp"
#include <string>
#include <unordered_map>

namespace cscpp::renderer {

class GLShader {
public:
    GLShader() = default;
    ~GLShader();
    
    /// Load and compile shader from source
    Result<void> loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);
    
    /// Load and compile shader from files
    Result<void> loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    
    /// Use this shader
    void bind() const;
    
    /// Stop using this shader
    static void unbind();
    
    /// Set uniform values
    void setUniform(const std::string& name, f32 value);
    void setUniform(const std::string& name, i32 value);
    void setUniform(const std::string& name, bool value);  // GLSL bool uniforms use i32
    void setUniform(const std::string& name, const Vec2& value);
    void setUniform(const std::string& name, const Vec3& value);
    void setUniform(const std::string& name, const Vec4& value);
    void setUniform(const std::string& name, const Mat4& value);
    
    /// Get shader program ID
    u32 getProgram() const { return m_program; }
    
    bool isValid() const { return m_program != 0; }
    
private:
    u32 compileShader(u32 type, const std::string& source);
    i32 getUniformLocation(const std::string& name);
    
    u32 m_program = 0;
    std::unordered_map<std::string, i32> m_uniformCache;
};

} // namespace cscpp::renderer

