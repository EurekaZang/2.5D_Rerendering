#pragma once

#include <string>
#include <cstdint>

namespace rgbd {
namespace render {

/**
 * OpenGL shader program wrapper
 */
class Shader {
public:
    Shader();
    ~Shader();
    
    // Non-copyable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    
    // Movable
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    
    /**
     * Load and compile shaders from source strings
     * @param vertexSource Vertex shader GLSL source
     * @param fragmentSource Fragment shader GLSL source
     * @return true on success
     */
    bool loadFromSource(const std::string& vertexSource,
                        const std::string& fragmentSource);
    
    /**
     * Load and compile shaders from files
     * @param vertexPath Path to vertex shader file
     * @param fragmentPath Path to fragment shader file
     * @return true on success
     */
    bool loadFromFiles(const std::string& vertexPath,
                       const std::string& fragmentPath);
    
    /**
     * Use this shader program
     */
    void use() const;
    
    /**
     * Get uniform location
     * @param name Uniform variable name
     * @return Location (-1 if not found)
     */
    int getUniformLocation(const std::string& name) const;
    
    /**
     * Set uniform values
     */
    void setUniform(const std::string& name, int value) const;
    void setUniform(const std::string& name, float value) const;
    void setUniform(const std::string& name, float x, float y) const;
    void setUniform(const std::string& name, float x, float y, float z) const;
    void setUniform(const std::string& name, float x, float y, float z, float w) const;
    void setUniformMatrix4(const std::string& name, const float* matrix) const;
    
    /**
     * Get program ID
     */
    uint32_t getProgramId() const { return programId_; }
    
    /**
     * Check if shader is valid
     */
    bool isValid() const { return programId_ != 0; }
    
    /**
     * Get last error message
     */
    const std::string& getError() const { return errorMsg_; }
    
    /**
     * Delete shader program
     */
    void destroy();
    
private:
    uint32_t programId_ = 0;
    std::string errorMsg_;
    
    /**
     * Compile a shader stage
     * @param source GLSL source code
     * @param type GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
     * @return Shader ID (0 on failure)
     */
    uint32_t compileShader(const std::string& source, uint32_t type);
    
    /**
     * Link shader program
     * @param vertexShader Compiled vertex shader ID
     * @param fragmentShader Compiled fragment shader ID
     * @return true on success
     */
    bool linkProgram(uint32_t vertexShader, uint32_t fragmentShader);
};

} // namespace render
} // namespace rgbd
