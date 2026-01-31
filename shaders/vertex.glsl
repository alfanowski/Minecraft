#version 410 core
layout (location = 0) in vec3 aPos;
out vec3 ourColor; // Passiamo il colore al fragment shader

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    // Usiamo la posizione come colore (normalizzata)
    ourColor = aPos + vec3(0.5, 0.5, 0.5);
}