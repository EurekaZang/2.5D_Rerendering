#version 330 core

// Input: Camera-space vertex position and texture coordinates
layout(location = 0) in vec3 aPosition;  // (X, Y, Z) in camera space
layout(location = 1) in vec2 aTexCoord;  // UV texture coordinates

// Uniform: Projection matrix
uniform mat4 uProjection;

// Output to fragment shader
out vec2 vTexCoord;
out float vDepth;

void main() {
    // Transform vertex from camera space to clip space
    gl_Position = uProjection * vec4(aPosition, 1.0);
    
    // Pass texture coordinates
    vTexCoord = aTexCoord;
    
    // Pass metric depth (camera Z)
    vDepth = aPosition.z;
}
