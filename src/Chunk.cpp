#define GL_SILENCE_DEPRECATION
#include "Chunk.hpp"
#include "FastNoiseLite.h"
#include <iostream>
#include <fstream>
#include <filesystem>

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
    // generateMesh viene chiamata da rebuild() con i vicini disponibili
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

    // Luminosità per faccia (simula luce direzionale dall'alto)
    float brightness = 0.8f; // default laterali
    if (faceType == "TOP")         brightness = 1.0f;
    else if (faceType == "BOTTOM") brightness = 0.5f;
    else if (faceType == "FRONT" || faceType == "BACK") brightness = 0.7f;
    else /* LEFT, RIGHT */         brightness = 0.8f;

    // Stride: 7 floats per vertice (pos3 + tex3 + brightness1)
    const auto startIdx = static_cast<unsigned int>(vertices.size() / 7);

    float x0 = static_cast<float>(x);
    float x1 = static_cast<float>(x + 1);
    float y0 = static_cast<float>(y);
    float y1 = static_cast<float>(y + 1);
    float z0 = static_cast<float>(z);
    float z1 = static_cast<float>(z + 1);
    float b = brightness;

    if (faceType == "TOP") {
        float f[] = { x0,y1,z1, 0,1,layer,b, x1,y1,z1, 1,1,layer,b, x1,y1,z0, 1,0,layer,b, x0,y1,z0, 0,0,layer,b };
        vertices.insert(vertices.end(), f, f + 28);
    }
    else if (faceType == "BOTTOM") {
        float f[] = { x0,y0,z0, 0,0,layer,b, x1,y0,z0, 1,0,layer,b, x1,y0,z1, 1,1,layer,b, x0,y0,z1, 0,1,layer,b };
        vertices.insert(vertices.end(), f, f + 28);
    }
    else if (faceType == "LEFT") {
        float f[] = { x0,y0,z0, 0,0,layer,b, x0,y0,z1, 1,0,layer,b, x0,y1,z1, 1,1,layer,b, x0,y1,z0, 0,1,layer,b };
        vertices.insert(vertices.end(), f, f + 28);
    }
    else if (faceType == "RIGHT") {
        float f[] = { x1,y0,z1, 0,0,layer,b, x1,y0,z0, 1,0,layer,b, x1,y1,z0, 1,1,layer,b, x1,y1,z1, 0,1,layer,b };
        vertices.insert(vertices.end(), f, f + 28);
    }
    else if (faceType == "FRONT") {
        float f[] = { x0,y0,z1, 0,0,layer,b, x1,y0,z1, 1,0,layer,b, x1,y1,z1, 1,1,layer,b, x0,y1,z1, 0,1,layer,b };
        vertices.insert(vertices.end(), f, f + 28);
    }
    else if (faceType == "BACK") {
        float f[] = { x1,y0,z0, 0,0,layer,b, x0,y0,z0, 1,0,layer,b, x0,y1,z0, 1,1,layer,b, x1,y1,z0, 0,1,layer,b };
        vertices.insert(vertices.end(), f, f + 28);
    }

    indices.push_back(startIdx + 0);
    indices.push_back(startIdx + 1);
    indices.push_back(startIdx + 2);
    indices.push_back(startIdx + 0);
    indices.push_back(startIdx + 2);
    indices.push_back(startIdx + 3);
}

void Chunk::generateMesh(const ChunkNeighbors& neighbors) {
    vertices.clear();
    indices.clear();

    // Helper: controlla se il blocco adiacente è aria, anche cross-chunk
    auto isAir = [&](int x, int y, int z) -> bool {
        if (y < 0 || y >= HEIGHT) return true;

        // Dentro il chunk corrente
        if (x >= 0 && x < SIZE && z >= 0 && z < SIZE)
            return blocks[x][y][z] == BlockType::AIR;

        // Cross-boundary: controlla chunk adiacente
        if (x < 0 && neighbors.left)
            return neighbors.left->blocks[SIZE - 1][y][z] == BlockType::AIR;
        if (x >= SIZE && neighbors.right)
            return neighbors.right->blocks[0][y][z] == BlockType::AIR;
        if (z < 0 && neighbors.back)
            return neighbors.back->blocks[x][y][SIZE - 1] == BlockType::AIR;
        if (z >= SIZE && neighbors.front)
            return neighbors.front->blocks[x][y][0] == BlockType::AIR;

        // Nessun vicino caricato: renderizza la faccia (sicuro)
        return true;
    };

    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            for (int z = 0; z < SIZE; z++) {
                unsigned char block = blocks[x][y][z];
                if (block == BlockType::AIR) continue;

                if (isAir(x, y+1, z)) addFace(x, y, z, "TOP", block);
                if (isAir(x, y-1, z)) addFace(x, y, z, "BOTTOM", block);
                if (isAir(x-1, y, z)) addFace(x, y, z, "LEFT", block);
                if (isAir(x+1, y, z)) addFace(x, y, z, "RIGHT", block);
                if (isAir(x, y, z+1)) addFace(x, y, z, "FRONT", block);
                if (isAir(x, y, z-1)) addFace(x, y, z, "BACK", block);
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

    int stride = 7 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    isUploaded = true;
    indexCount = static_cast<unsigned int>(indices.size());

    vertices.clear();
    indices.clear();
    vertices.shrink_to_fit();
    indices.shrink_to_fit();
}

void Chunk::rebuild(const ChunkNeighbors& neighbors) {
    if (isUploaded) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        isUploaded = false;
    }

    generateMesh(neighbors);
    upload();
}

void Chunk::rebuildMeshOnly(const ChunkNeighbors& neighbors) {
    generateMesh(neighbors);
    needsReupload = true;
}

void Chunk::reupload() {
    if (isUploaded) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        isUploaded = false;
    }
    upload();
    needsReupload = false;
}

void Chunk::render() const {
    if (!isUploaded) return;
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

bool Chunk::saveToFile(const std::string& worldDir) const {
    std::filesystem::create_directories(worldDir);
    std::string path = worldDir + "/chunk_" + std::to_string(chunkX) + "_" + std::to_string(chunkZ) + ".bin";
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(blocks), sizeof(blocks));
    return file.good();
}

bool Chunk::loadFromFile(const std::string& worldDir) {
    std::string path = worldDir + "/chunk_" + std::to_string(chunkX) + "_" + std::to_string(chunkZ) + ".bin";
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.read(reinterpret_cast<char*>(blocks), sizeof(blocks));
    return file.good();
}
