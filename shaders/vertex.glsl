#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aTexData; // U, V, Layer
layout (location = 2) in float aBrightness;

out vec3 TexCoords;
out float Brightness;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoords = aTexData;
    Brightness = aBrightness;
}
