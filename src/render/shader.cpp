#include "shader.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace rgbd {
namespace render {

Shader::Shader() {}

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept 
    : programId_(other.programId_)
    , errorMsg_(std::move(other.errorMsg_)) {
    other.programId_ = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        destroy();
        programId_ = other.programId_;
        errorMsg_ = std::move(other.errorMsg_);
        other.programId_ = 0;
    }
    return *this;
}

bool Shader::loadFromSource(const std::string& vertexSource,
                            const std::string& fragmentSource) {
    destroy();
    errorMsg_.clear();
    
    // Compile vertex shader
    uint32_t vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        return false;
    }
    
    // Compile fragment shader
    uint32_t fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }
    
    // Link program
    bool success = linkProgram(vertexShader, fragmentShader);
    
    // Clean up shaders (they're linked into the program now)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return success;
}

bool Shader::loadFromFiles(const std::string& vertexPath,
                           const std::string& fragmentPath) {
    // Read vertex shader
    std::ifstream vertexFile(vertexPath);
    if (!vertexFile.is_open()) {
        errorMsg_ = "Failed to open vertex shader file: " + vertexPath;
        std::cerr << errorMsg_ << std::endl;
        return false;
    }
    
    std::stringstream vertexStream;
    vertexStream << vertexFile.rdbuf();
    std::string vertexSource = vertexStream.str();
    
    // Read fragment shader
    std::ifstream fragmentFile(fragmentPath);
    if (!fragmentFile.is_open()) {
        errorMsg_ = "Failed to open fragment shader file: " + fragmentPath;
        std::cerr << errorMsg_ << std::endl;
        return false;
    }
    
    std::stringstream fragmentStream;
    fragmentStream << fragmentFile.rdbuf();
    std::string fragmentSource = fragmentStream.str();
    
    return loadFromSource(vertexSource, fragmentSource);
}

void Shader::use() const {
    if (programId_ != 0) {
        glUseProgram(programId_);
    }
}

int Shader::getUniformLocation(const std::string& name) const {
    if (programId_ == 0) return -1;
    return glGetUniformLocation(programId_, name.c_str());
}

void Shader::setUniform(const std::string& name, int value) const {
    int location = getUniformLocation(name);
    if (location >= 0) {
        glUniform1i(location, value);
    }
}

void Shader::setUniform(const std::string& name, float value) const {
    int location = getUniformLocation(name);
    if (location >= 0) {
        glUniform1f(location, value);
    }
}

void Shader::setUniform(const std::string& name, float x, float y) const {
    int location = getUniformLocation(name);
    if (location >= 0) {
        glUniform2f(location, x, y);
    }
}

void Shader::setUniform(const std::string& name, float x, float y, float z) const {
    int location = getUniformLocation(name);
    if (location >= 0) {
        glUniform3f(location, x, y, z);
    }
}

void Shader::setUniform(const std::string& name, float x, float y, float z, float w) const {
    int location = getUniformLocation(name);
    if (location >= 0) {
        glUniform4f(location, x, y, z, w);
    }
}

void Shader::setUniformMatrix4(const std::string& name, const float* matrix) const {
    int location = getUniformLocation(name);
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
    }
}

void Shader::destroy() {
    if (programId_ != 0) {
        glDeleteProgram(programId_);
        programId_ = 0;
    }
}

uint32_t Shader::compileShader(const std::string& source, uint32_t type) {
    uint32_t shader = glCreateShader(type);
    
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    // Check for errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        
        std::string log(logLength, '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, &log[0]);
        
        const char* typeName = (type == GL_VERTEX_SHADER) ? "Vertex" : "Fragment";
        errorMsg_ = std::string(typeName) + " shader compilation failed:\n" + log;
        std::cerr << errorMsg_ << std::endl;
        
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

bool Shader::linkProgram(uint32_t vertexShader, uint32_t fragmentShader) {
    programId_ = glCreateProgram();
    
    glAttachShader(programId_, vertexShader);
    glAttachShader(programId_, fragmentShader);
    glLinkProgram(programId_);
    
    // Check for errors
    GLint success;
    glGetProgramiv(programId_, GL_LINK_STATUS, &success);
    
    if (!success) {
        GLint logLength;
        glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &logLength);
        
        std::string log(logLength, '\0');
        glGetProgramInfoLog(programId_, logLength, nullptr, &log[0]);
        
        errorMsg_ = "Program linking failed:\n" + log;
        std::cerr << errorMsg_ << std::endl;
        
        glDeleteProgram(programId_);
        programId_ = 0;
        return false;
    }
    
    return true;
}

} // namespace render
} // namespace rgbd
