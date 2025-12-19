#version 450 core

in vec3 vPosition;
in vec3 vNormal;
in vec2 vTexCoord;

uniform vec3 uColor;
uniform bool uUseTexture;
uniform sampler2D uTexture;

out vec4 fragColor;

void main() {
    vec3 color = uColor;
    
    if (uUseTexture) {
        // Use standard texture sampling - OpenGL will automatically select appropriate mip level
        // The LOD bias we set in texture parameters will prefer higher quality mip levels
        vec4 texColor = texture(uTexture, vTexCoord);
        color = texColor.rgb;
        
        // Fallback for debugging
        if (dot(color, vec3(1.0)) < 0.001) {
            color = vec3(1.0, 0.0, 1.0);  // Magenta to indicate texture sampling issue
        }
    } else {
        // Visualize texture coordinates as a checkerboard pattern
        // This helps verify texture coordinates are working
        vec2 uv = vTexCoord;
        vec2 grid = floor(uv * 8.0);  // 8x8 grid
        float checker = mod(grid.x + grid.y, 2.0);
        vec3 texColor = mix(vec3(0.3, 0.3, 0.4), vec3(0.6, 0.6, 0.7), checker);
        
        // Blend with base color
        color = mix(uColor, texColor, 0.7);
    }
    
    // Simple lighting (brightened to make textures more visible)
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float ndotl = max(dot(normalize(vNormal), lightDir), 0.6);  // Increased minimum from 0.3 to 0.6
    color *= ndotl;
    
    // Additional brightness boost for textures
    if (uUseTexture) {
        color *= 1.2;  // 20% brightness boost
    }
    
    fragColor = vec4(color, 1.0);
}

