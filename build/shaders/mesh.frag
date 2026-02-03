#version 330 core

// Input from vertex shader
in vec2 vTexCoord;
in float vDepth;

// Uniform: RGB texture
uniform sampler2D uRGBTexture;

// Multiple Render Targets (MRT)
layout(location = 0) out vec4 outColor;     // RGB output (RGBA8)
layout(location = 1) out float outDepth;    // Metric depth output (R32F)
layout(location = 2) out float outMask;     // Validity mask output (R8)

void main() {
    // Sample RGB texture at interpolated texture coordinates
    vec4 color = texture(uRGBTexture, vTexCoord);
    
    // Output RGB color
    outColor = color;
    
    // Output metric depth (camera-space Z in meters)
    outDepth = vDepth;
    
    // Output validity mask (1.0 = this pixel was rendered)
    outMask = 1.0;
}
