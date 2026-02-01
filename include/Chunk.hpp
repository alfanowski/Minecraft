#ifndef CHUNK_H
#define CHUNK_H

#include <vector>
#include <string>
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>

// Helper Hash per le coordinate dei chunk
inline long long chunkHash(int x, int z) {
    return (static_cast<long long>(x) << 32) | (static_cast<unsigned int>(z));
}

class Chunk {
public:
    static const int SIZE = 16;
    static const int HEIGHT = 128; // Aumentato a 128!

    // 0 = Aria, 1 = Erba, 2 = Terra, 3 = Pietra, 4 = Bedrock
    unsigned char blocks[SIZE][HEIGHT][SIZE]{};

    int chunkX, chunkZ;

    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    void buildMesh();
    void render() const;

    // Helper per il Frustum Culling
    glm::vec3 getMin() const { return glm::vec3(chunkX * SIZE, 0, chunkZ * SIZE); }
    glm::vec3 getMax() const { return glm::vec3((chunkX + 1) * SIZE, HEIGHT, (chunkZ + 1) * SIZE); }

private:
    unsigned int VAO{}, VBO{}, EBO{};
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    void addFace(int x, int y, int z, std::string faceType, unsigned char blockID);
};

#endif