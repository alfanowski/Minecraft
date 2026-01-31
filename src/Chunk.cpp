#include "Chunk.hpp"
#include <iostream>

Chunk::Chunk() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Inizializzazione ignorante del terreno (piano piatto a metà altezza)
    for (auto & block : blocks) {
        for (int z = 0; z < SIZE; z++) {
            for (int y = 0; y < SIZE; y++) {
                block[y][z] = (y < 8) ? 1 : 0;
            }
        }
    }
    buildMesh();
}

void Chunk::addFace(int x, int y, int z, const std::string& faceType) {
    float uMin = 0.0f, uMax = 1.0f, vMin = 0.0f, vMax = 1.0f;

    // Offset per gli indici: quanti vertici abbiamo già inserito?
    auto startIdx = static_cast<unsigned int>(vertices.size() / 5);

    if (faceType == "TOP") {
        float f[] = {
            x-0.5f, y+0.5f, z+0.5f, uMin, vMin, // 0
            x+0.5f, y+0.5f, z+0.5f, uMax, vMin, // 1
            x+0.5f, y+0.5f, z-0.5f, uMax, vMax, // 2
            x-0.5f, y+0.5f, z-0.5f, uMin, vMax  // 3
        };
        vertices.insert(vertices.end(), f, f + 20);
    }
    else if (faceType == "BOTTOM") {
        float f[] = {
            x-0.5f, y-0.5f, z-0.5f, uMin, vMin,
            x+0.5f, y-0.5f, z-0.5f, uMax, vMin,
            x+0.5f, y-0.5f, z+0.5f, uMax, vMax,
            x-0.5f, y-0.5f, z+0.5f, uMin, vMax
        };
        vertices.insert(vertices.end(), f, f + 20);
    }
    else if (faceType == "LEFT") {
        float f[] = {
            x-0.5f, y+0.5f, z+0.5f, uMax, vMax,
            x-0.5f, y+0.5f, z-0.5f, uMin, vMax,
            x-0.5f, y-0.5f, z-0.5f, uMin, vMin,
            x-0.5f, y-0.5f, z+0.5f, uMax, vMin
        };
        vertices.insert(vertices.end(), f, f + 20);
    }
    else if (faceType == "RIGHT") {
        float f[] = {
            x+0.5f, y+0.5f, z-0.5f, uMax, vMax,
            x+0.5f, y+0.5f, z+0.5f, uMin, vMax,
            x+0.5f, y-0.5f, z+0.5f, uMin, vMin,
            x+0.5f, y-0.5f, z-0.5f, uMax, vMin
        };
        vertices.insert(vertices.end(), f, f + 20);
    }
    else if (faceType == "FRONT") {
        float f[] = {
            x-0.5f, y+0.5f, z+0.5f, uMin, vMax,
            x+0.5f, y+0.5f, z+0.5f, uMax, vMax,
            x+0.5f, y-0.5f, z+0.5f, uMax, vMin,
            x-0.5f, y-0.5f, z+0.5f, uMin, vMin
        };
        vertices.insert(vertices.end(), f, f + 20);
    }
    else if (faceType == "BACK") {
        float f[] = {
            x+0.5f, y+0.5f, z-0.5f, uMin, vMax,
            x-0.5f, y+0.5f, z-0.5f, uMax, vMax,
            x-0.5f, y-0.5f, z-0.5f, uMax, vMin,
            x+0.5f, y-0.5f, z-0.5f, uMin, vMin
        };
        vertices.insert(vertices.end(), f, f + 20);
    }

    // Aggiungiamo gli indici per i due triangoli della faccia (0-1-2 e 2-3-0)
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
        for (int y = 0; y < SIZE; y++) {
            for (int z = 0; z < SIZE; z++) {
                if (blocks[x][y][z] == 0) continue;

                // Face Culling Razionale
                if (y == SIZE - 1 || blocks[x][y+1][z] == 0) addFace(x, y, z, "TOP");
                if (y == 0        || blocks[x][y-1][z] == 0) addFace(x, y, z, "BOTTOM");
                if (x == 0        || blocks[x-1][y][z] == 0) addFace(x, y, z, "LEFT");
                if (x == SIZE - 1 || blocks[x+1][y][z] == 0) addFace(x, y, z, "RIGHT");
                if (z == SIZE - 1 || blocks[x][y][z+1] == 0) addFace(x, y, z, "FRONT");
                if (z == 0        || blocks[x][y][z-1] == 0) addFace(x, y, z, "BACK");
            }
        }
    }

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void Chunk::render() const {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
}

Chunk::~Chunk() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    // Pulizia dei vettori (opzionale ma pulito)
    vertices.clear();
    indices.clear();
}