#ifndef CHUNK_H
#define CHUNK_H

#include <vector>
#include <string>
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>

// --- COSTANTI GLOBALI ---
namespace BlockType {
    constexpr unsigned char AIR      = 0;
    constexpr unsigned char GRASS    = 1;
    constexpr unsigned char DIRT     = 2;
    constexpr unsigned char STONE    = 3;
    constexpr unsigned char BEDROCK  = 4;
    constexpr unsigned char COUNT    = 5; // Numero totale di tipi
}

namespace TextureLayer {
    constexpr float GRASS_TOP  = 0.0f;
    constexpr float GRASS_SIDE = 1.0f;
    constexpr float DIRT       = 2.0f;
    constexpr float STONE      = 3.0f;
    constexpr float BEDROCK    = 4.0f;
}

namespace WorldConfig {
    constexpr int RENDER_DISTANCE      = 8;
    constexpr int UNLOAD_DISTANCE      = 10;
    constexpr int INITIAL_LOAD_RADIUS  = 2;
    constexpr int UPLOADS_PER_FRAME    = 16;
    constexpr float INTERACTION_RANGE  = 5.0f;
    constexpr float NOISE_FREQUENCY    = 0.01f;
    constexpr float TERRAIN_BASE       = 30.0f;
    constexpr float TERRAIN_AMPLITUDE  = 20.0f;
}

namespace PlayerConfig {
    constexpr float WIDTH       = 0.5f;
    constexpr float HEIGHT      = 1.8f;
    constexpr float EYE_HEIGHT  = 1.62f;
    constexpr float MOVE_SPEED  = 4.3f;
    constexpr float MOUSE_SENS  = 0.1f;
    constexpr float GRAVITY     = -28.0f;
    constexpr float JUMP_HEIGHT = 1.2f;
    constexpr float MAX_FALL_SPEED = -50.0f;
}

inline long long chunkHash(int x, int z) {
    return (static_cast<long long>(x) << 32) | (static_cast<unsigned int>(z));
}

class Chunk {
public:
    static const int SIZE = 16;
    static const int HEIGHT = 128;

    unsigned char blocks[SIZE][HEIGHT][SIZE]{};

    int chunkX, chunkZ;
    bool isUploaded = false;
    unsigned int indexCount = 0;

    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    void generate();
    void upload();
    void render() const;

    void rebuild();

    glm::vec3 getMin() const { return glm::vec3(chunkX * SIZE, 0, chunkZ * SIZE); }
    glm::vec3 getMax() const { return glm::vec3((chunkX + 1) * SIZE, HEIGHT, (chunkZ + 1) * SIZE); }

private:
    unsigned int VAO{}, VBO{}, EBO{};

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    void generateTerrain();
    void generateMesh();
    void addFace(int x, int y, int z, std::string faceType, unsigned char blockID);
};

#endif