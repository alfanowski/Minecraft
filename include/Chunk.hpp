#ifndef CHUNK_H
#define CHUNK_H

#include <vector>
#include <string>
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>

class Chunk {
public:
    // Dimensioni standard dei chunk di Minecraft
    static const int SIZE = 16;
    static const int HEIGHT = 16;

    // 0 = Aria, 1 = Terra (per ora usiamo un sistema ignorante a 8 bit)
    unsigned char blocks[SIZE][HEIGHT][SIZE]{};

    int chunkX, chunkZ;

    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    // Genera la mesh basandosi sui blocchi adiacenti (Face Culling)
    void buildMesh();

    // Disegna il chunk a schermo
    void render() const;

private:
    // OpenGL ID per i buffer
    unsigned int VAO{}, VBO{}, EBO{};

    // Dati dinamici della mesh
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Helper per aggiungere una faccia alla mesh se visibile
    void addFace(int x, int y, int z, std::string faceType, unsigned char blockID);};

#endif