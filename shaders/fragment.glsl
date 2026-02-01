#version 410 core
out vec4 FragColor;

in vec3 TexCoords;
uniform sampler2DArray textureArray;

void main() {
    vec4 texColor = texture(textureArray, TexCoords);

    // Se stiamo renderizzando il Layer 0 (Grass Top)
    if (abs(TexCoords.z - 0.0) < 0.1) {
        vec3 grassTint = vec3(0.60, 0.95, 0.40);
        texColor.rgb *= grassTint;
    }

    if(texColor.a < 0.1) discard;
    FragColor = texColor;
}