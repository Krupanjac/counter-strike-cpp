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
        
        // Subtle texture sharpening using unsharp mask technique
        // This enhances texture detail without being too aggressive
        vec2 texelSize = 1.0 / textureSize(uTexture, 0);
        
        // Sample neighboring pixels for edge detection
        vec4 texColorX = texture(uTexture, vTexCoord + vec2(texelSize.x, 0.0));
        vec4 texColorY = texture(uTexture, vTexCoord + vec2(0.0, texelSize.y));
        vec4 texColorAvg = (texColorX + texColorY) * 0.5;
        
        // Subtle sharpening: enhance edges while preserving smooth areas
        // 0.1-0.2 is a good range for subtle enhancement without artifacts
        vec3 sharpened = texColor.rgb + (texColor.rgb - texColorAvg.rgb) * 0.12;
        color = clamp(sharpened, 0.0, 1.0);
        
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

