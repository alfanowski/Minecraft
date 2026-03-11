#define GL_SILENCE_DEPRECATION
#include "Chunk.hpp"
#include "FastNoiseLite.h"
#include <iostream>

Chunk::Chunk(int cx, int cz) : chunkX(cx), chunkZ(cz) {
}

Chunk::~Chunk() {
    if (isUploaded) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
}

void Chunk::generate() {
    generateTerrain();
    generateMesh();
}

void Chunk::generateTerrain() {
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(WorldConfig::NOISE_FREQUENCY);

    for (int x = 0; x < SIZE; x++) {
        for (int z = 0; z < SIZE; z++) {
            float worldX = static_cast<float>(x + chunkX * SIZE);
            float worldZ = static_cast<float>(z + chunkZ * SIZE);

            float noiseValue = noise.GetNoise(worldX, worldZ);
            int terrainHeight = static_cast<int>((noiseValue + 1.0f) * WorldConfig::TERRAIN_AMPLITUDE + WorldConfig::TERRAIN_BASE);

            for (int y = 0; y < HEIGHT; y++) {
                if (y == 0) {
                    blocks[x][y][z] = BlockType::BEDROCK;
                } else if (y < terrainHeight - 3) {
                    blocks[x][y][z] = BlockType::STONE;
                } else if (y < terrainHeight) {
                    blocks[x][y][z] = BlockType::DIRT;
                } else if (y == terrainHeight) {
                    blocks[x][y][z] = BlockType::GRASS;
                } else {
                    blocks[x][y][z] = BlockType::AIR;
                }
            }
        }
    }
}

void Chunk::addFace(int x, int y, int z, std::string faceType, unsigned char blockID) {
    float layer = 0.0f;

    if (blockID == BlockType::GRASS) {
        if (faceType == "TOP") layer = TextureLayer::GRASS_TOP;
        else if (faceType == "BOTTOM") layer = TextureLayer::DIRT;
        else layer = TextureLayer::GRASS_SIDE;
    } else if (blockID == BlockType::DIRT) {
        layer = TextureLayer::DIRT;
    } else if (blockID == BlockType::STONE) {
        layer = TextureLayer::STONE;
    } else if (blockID == BlockType::BEDROCK) {
        layer = TextureLayer::BEDROCK;
    }

    const auto startIdx = static_cast<unsigned int>(vertices.size() / 6);

    float x0 = static_cast<float>(x);
    float x1 = static_cast<float>(x + 1);
    float y0 = static_cast<float>(y);
    float y1 = static_cast<float>(y + 1);
    float z0 = static_cast<float>(z);
    float z1 = static_cast<float>(z + 1);

    // Definizione vertici CCW
    if (faceType == "TOP") {
        float f[] = { x0, y1, z1, 0.0f, 1.0f, layer, x1, y1, z1, 1.0f, 1.0f, layer, x1, y1, z0, 1.0f, 0.0f, layer, x0, y1, z0, 0.0f, 0.0f, layer };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "BOTTOM") {
        float f[] = { x0, y0, z0, 0.0f, 0.0f, layer, x1, y0, z0, 1.0f, 0.0f, layer, x1, y0, z1, 1.0f, 1.0f, layer, x0, y0, z1, 0.0f, 1.0f, layer };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "LEFT") {
        float f[] = { x0, y0, z0, 0.0f, 0.0f, layer, x0, y0, z1, 1.0f, 0.0f, layer, x0, y1, z1, 1.0f, 1.0f, layer, x0, y1, z0, 0.0f, 1.0f, layer };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "RIGHT") {
        float f[] = { x1, y0, z1, 0.0f, 0.0f, layer, x1, y0, z0, 1.0f, 0.0f, layer, x1, y1, z0, 1.0f, 1.0f, layer, x1, y1, z1, 0.0f, 1.0f, layer };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "FRONT") {
        float f[] = { x0, y0, z1, 0.0f, 0.0f, layer, x1, y0, z1, 1.0f, 0.0f, layer, x1, y1, z1, 1.0f, 1.0f, layer, x0, y1, z1, 0.0f, 1.0f, layer };
        vertices.insert(vertices.end(), f, f + 24);
    }
    else if (faceType == "BACK") {
        float f[] = { x1, y0, z0, 0.0f, 0.0f, layer, x0, y0, z0, 1.0f, 0.0f, layer, x0, y1, z0, 1.0f, 1.0f, layer, x1, y1, z0, 0.0f, 1.0f, layer };
        vertices.insert(vertices.end(), f, f + 24);
    }

    indices.push_back(startIdx + 0);
    indices.push_back(startIdx + 1);
    indices.push_back(startIdx + 2);
    indices.push_back(startIdx + 0);
    indices.push_back(startIdx + 2);
    indices.push_back(startIdx + 3);
}

void Chunk::generateMesh() {
    vertices.clear();
    indices.clear();

    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int z = 0; z < SIZE; z++) {
                unsigned char block = blocks[x][y][z];
                if (block == BlockType::AIR) continue;

                if (y == HEIGHT - 1 || blocks[x][y+1][z] == BlockType::AIR) addFace(x, y, z, "TOP", block);
                if (y == 0          || blocks[x][y-1][z] == BlockType::AIR) addFace(x, y, z, "BOTTOM", block);
                if (x == 0          || blocks[x-1][y][z] == BlockType::AIR) addFace(x, y, z, "LEFT", block);
                if (x == SIZE - 1   || blocks[x+1][y][z] == BlockType::AIR) addFace(x, y, z, "RIGHT", block);
                if (z == SIZE - 1   || blocks[x][y][z+1] == BlockType::AIR) addFace(x, y, z, "FRONT", block);
                if (z == 0          || blocks[x][y][z-1] == BlockType::AIR) addFace(x, y, z, "BACK", block);
            }
        }
    }
}

void Chunk::upload() {
    if (vertices.empty()) return;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    isUploaded = true;
    indexCount = static_cast<unsigned int>(indices.size());

    vertices.clear();
    indices.clear();
    vertices.shrink_to_fit();
    indices.shrink_to_fit();
}

// NUOVO: Ricostruisce la mesh e aggiorna la GPU
void Chunk::rebuild() {
    // 1. Pulisci vecchi buffer se esistono
    if (isUploaded) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        isUploaded = false;
    }

    // 2. Rigenera mesh (CPU)
    generateMesh();

    // 3. Carica su GPU
    upload();
}

void Chunk::render() const {
    if (!isUploaded) return;
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}
