#include "Chunk.hpp"
#include "FastNoiseLite.h"
#include <iostream>

Chunk::Chunk(int cx, int cz) : chunkX(cx), chunkZ(cz) {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Inizializzazione Noise professionale
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(0.05f);

    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            // Coordinate globali per un terreno continuo
            float worldX = static_cast<float>(x + chunkX * SIZE);
            float worldZ = static_cast<float>(z + chunkZ * SIZE);
            float noiseValue = noise.GetNoise(worldX, worldZ);

            // Mappatura altezza (tra 4 e 12 blocchi)
            int terrainHeight = static_cast<int>((noiseValue + 1.0f) * 4.0f + 4.0f);

            for (int y = 0; y < HEIGHT; y++) {
                if (y < terrainHeight) {
                    blocks[x][y][z] = 2; // Terra
                } else if (y == terrainHeight) {
                    blocks[x][y][z] = 1; // Erba
                } else {
                    blocks[x][y][z] = 0; // Aria
                }
            }
        }
    }
    buildMesh();
}

Chunk::~Chunk() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Chunk::addFace(int x, int y, int z, std::string faceType, unsigned char blockID) {
    float layer = 0.0f;

    // Mapping dei Layer
    // 0: grass_top, 1: grass_side, 2: dirt
    if (blockID == 1) { // Blocco Erba
        if (faceType == "TOP") layer = 0.0f;
        else if (faceType == "BOTTOM") layer = 2.0f;
        else layer = 1.0f;
    } else if (blockID == 2) { // Blocco Terra
        layer = 2.0f;
    }

    unsigned int startIdx = static_cast<unsigned int>(vertices.size() / 6);

    // Coordinate vertici allineate alla griglia 0..1 (invece di -0.5..0.5)
    // Questo allinea la grafica con la logica di collisione basata su floor()
    float x0 = static_cast<float>(x);
    float x1 = static_cast<float>(x + 1);
    float y0 = static_cast<float>(y);
    float y1 = static_cast<float>(y + 1);
    float z0 = static_cast<float>(z);
    float z1 = static_cast<float>(z + 1);

    // Definizione vertici: X, Y, Z, U, V, Layer
    if (faceType == "TOP") {
        float f[] = {
            x0, y1, z1, 0.0f, 1.0f, layer,
            x1, y1, z1, 1.0f, 1.0f, layer,
            x1, y1, z0, 1.0f, 0.0f, layer,
            x0, y1, z0, 0.0f, 0.0f, layer
        };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "BOTTOM") {
        float f[] = {
            x0, y0, z0, 0.0f, 0.0f, layer,
            x1, y0, z0, 1.0f, 0.0f, layer,
            x1, y0, z1, 1.0f, 1.0f, layer,
            x0, y0, z1, 0.0f, 1.0f, layer
        };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "LEFT") {
        float f[] = {
            x0, y1, z0, 0.0f, 1.0f, layer,
            x0, y1, z1, 1.0f, 1.0f, layer,
            x0, y0, z1, 1.0f, 0.0f, layer,
            x0, y0, z0, 0.0f, 0.0f, layer
        };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "RIGHT") {
        float f[] = {
            x1, y1, z1, 0.0f, 1.0f, layer,
            x1, y1, z0, 1.0f, 1.0f, layer,
            x1, y0, z0, 1.0f, 0.0f, layer,
            x1, y0, z1, 0.0f, 0.0f, layer
        };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "FRONT") {
        float f[] = {
            x0, y1, z1, 0.0f, 1.0f, layer,
            x1, y1, z1, 1.0f, 1.0f, layer,
            x1, y0, z1, 1.0f, 0.0f, layer,
            x0, y0, z1, 0.0f, 0.0f, layer
        };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "BACK") {
        float f[] = {
            x1, y1, z0, 0.0f, 1.0f, layer,
            x0, y1, z0, 1.0f, 1.0f, layer,
            x0, y0, z0, 1.0f, 0.0f, layer,
            x1, y0, z0, 0.0f, 0.0f, layer
        };
        vertices.insert(vertices.end(), f, f + 24);
    }

    indices.push_back(startIdx + 0);
    indices.push_back(startIdx + 1);
    indices.push_back(startIdx + 2);
    indices.push_back(startIdx + 2);
    indices.push_back(startIdx + 3);
    indices.push_back(startIdx + 0);
}

void Chunk::buildMesh() {
    vertices.clear();
    indices.clear();

    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int z = 0; z < SIZE; z++) {
                unsigned char block = blocks[x][y][z];
                if (block == 0) continue;

                // Face Culling Interno
                if (y == HEIGHT - 1 || blocks[x][y+1][z] == 0) addFace(x, y, z, "TOP", block);
                if (y == 0          || blocks[x][y-1][z] == 0) addFace(x, y, z, "BOTTOM", block);
                if (x == 0          || blocks[x-1][y][z] == 0) addFace(x, y, z, "LEFT", block);
                if (x == SIZE - 1   || blocks[x+1][y][z] == 0) addFace(x, y, z, "RIGHT", block);
                if (z == SIZE - 1   || blocks[x][y][z+1] == 0) addFace(x, y, z, "FRONT", block);
                if (z == 0          || blocks[x][y][z-1] == 0) addFace(x, y, z, "BACK", block);
            }
        }
    }

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Attributi: 0 = Posizione (3 float), 1 = TexCoords (3 float: U, V, Layer)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void Chunk::render() const {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
}
