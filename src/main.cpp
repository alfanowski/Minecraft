#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <unordered_map>
#include <future>
#include <list>
#include <unordered_set>
#include "Shader.hpp"
#include "Camera.hpp"
#include "Chunk.hpp"
#include "ThreadPool.hpp"
#include "stb_image.h"

// --- GLOBALI ---
Camera camera(glm::vec3(8.0f, 80.0f, 30.0f));
std::unordered_map<long long, std::unique_ptr<Chunk>> worldChunks;
ThreadPool chunkThreadPool(std::max(2u, std::thread::hardware_concurrency() - 1));

struct PendingChunk {
    long long key;
    int x, z;
    std::future<void> task;
};
std::list<PendingChunk> generationQueue;
std::unordered_set<long long> queuedKeys; // O(1) lookup per evitare duplicati
std::vector<Chunk*> uploadQueue;

// Coda per rebuild asincroni (break/place blocchi)
struct PendingRebuild {
    std::vector<Chunk*> chunks;
    std::future<void> task;
};
std::list<PendingRebuild> rebuildQueue;

float lastX = 640.0f, lastY = 360.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Blocco selezionato per il piazzamento
const unsigned char placeableBlocks[] = { BlockType::GRASS, BlockType::DIRT, BlockType::STONE, BlockType::BEDROCK };
const char* blockNames[] = { "Grass", "Dirt", "Stone", "Bedrock" };
const int PLACEABLE_COUNT = 4;
int selectedBlockIndex = 2; // Default: pietra

// Render distance ora in WorldConfig::RENDER_DISTANCE

// --- SHADER MIRINO ---
const char* crosshairVS = R"(
#version 410 core
layout (location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)";

const char* crosshairFS = R"(
#version 410 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

unsigned int crosshairVAO, crosshairVBO, crosshairProgram;

void setupCrosshair() {
    float vertices[] = {
        -0.02f,  0.0f, 0.02f,  0.0f,
         0.0f,  -0.035f, 0.0f,   0.035f
    };
    glGenVertexArrays(1, &crosshairVAO);
    glGenBuffers(1, &crosshairVBO);
    glBindVertexArray(crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &crosshairVS, NULL);
    glCompileShader(vertex);
    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &crosshairFS, NULL);
    glCompileShader(fragment);
    crosshairProgram = glCreateProgram();
    glAttachShader(crosshairProgram, vertex);
    glAttachShader(crosshairProgram, fragment);
    glLinkProgram(crosshairProgram);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void drawCrosshair() {
    glUseProgram(crosshairProgram);
    glBindVertexArray(crosshairVAO);
    glDrawArrays(GL_LINES, 0, 4);
}

// --- HELPER: costruisce i vicini di un chunk ---
ChunkNeighbors getNeighbors(int cx, int cz) {
    ChunkNeighbors n;
    auto it = worldChunks.find(chunkHash(cx - 1, cz));
    if (it != worldChunks.end()) n.left = it->second.get();
    it = worldChunks.find(chunkHash(cx + 1, cz));
    if (it != worldChunks.end()) n.right = it->second.get();
    it = worldChunks.find(chunkHash(cx, cz + 1));
    if (it != worldChunks.end()) n.front = it->second.get();
    it = worldChunks.find(chunkHash(cx, cz - 1));
    if (it != worldChunks.end()) n.back = it->second.get();
    return n;
}

// --- GESTIONE MONDO ASINCRONA ---
void updateChunks() {
    int playerChunkX = static_cast<int>(floor(camera.Position.x / 16.0f));
    int playerChunkZ = static_cast<int>(floor(camera.Position.z / 16.0f));

    for (int x = playerChunkX - WorldConfig::RENDER_DISTANCE; x <= playerChunkX + WorldConfig::RENDER_DISTANCE; x++) {
        for (int z = playerChunkZ - WorldConfig::RENDER_DISTANCE; z <= playerChunkZ + WorldConfig::RENDER_DISTANCE; z++) {
            long long key = chunkHash(x, z);

            if (worldChunks.find(key) == worldChunks.end() && queuedKeys.find(key) == queuedKeys.end()) {
                worldChunks[key] = std::make_unique<Chunk>(x, z);
                Chunk* chunkPtr = worldChunks[key].get();
                queuedKeys.insert(key);

                generationQueue.push_back({
                    key, x, z,
                    chunkThreadPool.submit([chunkPtr]() {
                        chunkPtr->generate();
                    })
                });
            }
        }
    }

    for (auto it = generationQueue.begin(); it != generationQueue.end(); ) {
        if (it->task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            long long key = it->key;
            queuedKeys.erase(key);
            if (worldChunks.find(key) != worldChunks.end()) {
                uploadQueue.push_back(worldChunks[key].get());
            }
            it = generationQueue.erase(it);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < WorldConfig::UPLOADS_PER_FRAME && !uploadQueue.empty(); i++) {
        Chunk* chunk = uploadQueue.back();
        uploadQueue.pop_back();
        chunk->rebuild(getNeighbors(chunk->chunkX, chunk->chunkZ));
    }

    // Processa rebuild asincroni completati (GPU upload)
    for (auto it = rebuildQueue.begin(); it != rebuildQueue.end(); ) {
        if (it->task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            for (Chunk* chunk : it->chunks) {
                if (chunk->needsReupload) chunk->reupload();
            }
            it = rebuildQueue.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = worldChunks.begin(); it != worldChunks.end(); ) {
        int cx = it->second->chunkX;
        int cz = it->second->chunkZ;
        if (abs(cx - playerChunkX) > WorldConfig::UNLOAD_DISTANCE ||
            abs(cz - playerChunkZ) > WorldConfig::UNLOAD_DISTANCE) {
            it = worldChunks.erase(it);
        } else {
            ++it;
        }
    }
}

void forceLoadInitialChunks() {
    int playerChunkX = static_cast<int>(floor(camera.Position.x / 16.0f));
    int playerChunkZ = static_cast<int>(floor(camera.Position.z / 16.0f));
    int initialRadius = WorldConfig::INITIAL_LOAD_RADIUS;

    // Prima genera tutto il terreno
    for (int x = playerChunkX - initialRadius; x <= playerChunkX + initialRadius; x++) {
        for (int z = playerChunkZ - initialRadius; z <= playerChunkZ + initialRadius; z++) {
            long long key = chunkHash(x, z);
            if (worldChunks.find(key) == worldChunks.end()) {
                worldChunks[key] = std::make_unique<Chunk>(x, z);
                worldChunks[key]->generateTerrain();
            }
        }
    }
    // Poi costruisci mesh con vicini disponibili e carica su GPU
    for (int x = playerChunkX - initialRadius; x <= playerChunkX + initialRadius; x++) {
        for (int z = playerChunkZ - initialRadius; z <= playerChunkZ + initialRadius; z++) {
            long long key = chunkHash(x, z);
            worldChunks[key]->rebuild(getNeighbors(x, z));
        }
    }
}

// --- DDA Raycast (Amanatides-Woo) ---
// Restituisce il blocco colpito e il blocco precedente (per il piazzamento)
struct RaycastResult {
    bool hit = false;
    int x, y, z;           // Blocco colpito
    int prevX, prevY, prevZ; // Blocco precedente (aria)
};

RaycastResult raycast(glm::vec3 origin, glm::vec3 dir, float maxDist) {
    RaycastResult result;

    // Posizione iniziale nel grid
    int x = static_cast<int>(floor(origin.x));
    int y = static_cast<int>(floor(origin.y));
    int z = static_cast<int>(floor(origin.z));

    // Direzione di step (+1 o -1) per ogni asse
    int stepX = (dir.x >= 0) ? 1 : -1;
    int stepY = (dir.y >= 0) ? 1 : -1;
    int stepZ = (dir.z >= 0) ? 1 : -1;

    // Distanza t per attraversare una cella intera per asse
    float tDeltaX = (dir.x != 0.0f) ? std::abs(1.0f / dir.x) : 1e30f;
    float tDeltaY = (dir.y != 0.0f) ? std::abs(1.0f / dir.y) : 1e30f;
    float tDeltaZ = (dir.z != 0.0f) ? std::abs(1.0f / dir.z) : 1e30f;

    // Distanza t al prossimo bordo di cella
    float tMaxX = (dir.x != 0.0f) ? ((dir.x > 0 ? (x + 1.0f - origin.x) : (origin.x - x)) * tDeltaX) : 1e30f;
    float tMaxY = (dir.y != 0.0f) ? ((dir.y > 0 ? (y + 1.0f - origin.y) : (origin.y - y)) * tDeltaY) : 1e30f;
    float tMaxZ = (dir.z != 0.0f) ? ((dir.z > 0 ? (z + 1.0f - origin.z) : (origin.z - z)) * tDeltaZ) : 1e30f;

    result.prevX = x; result.prevY = y; result.prevZ = z;

    float t = 0.0f;
    while (t < maxDist) {
        // Controlla blocco corrente
        int chunkX = static_cast<int>(floor(x / 16.0f));
        int chunkZ = static_cast<int>(floor(z / 16.0f));
        auto it = worldChunks.find(chunkHash(chunkX, chunkZ));

        if (it != worldChunks.end() && y >= 0 && y < Chunk::HEIGHT) {
            int localX = x % 16; if (localX < 0) localX += 16;
            int localZ = z % 16; if (localZ < 0) localZ += 16;
            if (it->second->blocks[localX][y][localZ] != BlockType::AIR) {
                result.hit = true;
                result.x = x; result.y = y; result.z = z;
                return result;
            }
        }

        // Salva posizione precedente
        result.prevX = x; result.prevY = y; result.prevZ = z;

        // Avanza al prossimo bordo di cella
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                t = tMaxX; x += stepX; tMaxX += tDeltaX;
            } else {
                t = tMaxZ; z += stepZ; tMaxZ += tDeltaZ;
            }
        } else {
            if (tMaxY < tMaxZ) {
                t = tMaxY; y += stepY; tMaxY += tDeltaY;
            } else {
                t = tMaxZ; z += stepZ; tMaxZ += tDeltaZ;
            }
        }
    }
    return result;
}

// Helper: ricostruisci chunk e adiacenti al bordo dopo modifica blocco
void rebuildChunkAndBorders(int blockX, int blockY, int blockZ) {
    int chunkX = static_cast<int>(floor(blockX / 16.0f));
    int chunkZ = static_cast<int>(floor(blockZ / 16.0f));
    int localX = blockX % 16; if (localX < 0) localX += 16;
    int localZ = blockZ % 16; if (localZ < 0) localZ += 16;

    // Raccogli i chunk da ricostruire e i loro vicini
    struct RebuildInfo { Chunk* chunk; ChunkNeighbors neighbors; };
    std::vector<RebuildInfo> toRebuild;

    auto addIfExists = [&](int cx, int cz) {
        auto it = worldChunks.find(chunkHash(cx, cz));
        if (it != worldChunks.end())
            toRebuild.push_back({it->second.get(), getNeighbors(cx, cz)});
    };

    addIfExists(chunkX, chunkZ);
    if (localX == 0) addIfExists(chunkX-1, chunkZ);
    if (localX == 15) addIfExists(chunkX+1, chunkZ);
    if (localZ == 0) addIfExists(chunkX, chunkZ-1);
    if (localZ == 15) addIfExists(chunkX, chunkZ+1);

    // Lancia mesh generation asincrona
    std::vector<Chunk*> chunks;
    for (auto& ri : toRebuild) chunks.push_back(ri.chunk);

    rebuildQueue.push_back({
        chunks,
        chunkThreadPool.submit([toRebuild = std::move(toRebuild)]() {
            for (auto& ri : toRebuild)
                ri.chunk->rebuildMeshOnly(ri.neighbors);
        })
    });
}

void breakBlock() {
    auto result = raycast(camera.Position, camera.Front, WorldConfig::INTERACTION_RANGE);
    if (!result.hit) return;

    int chunkX = static_cast<int>(floor(result.x / 16.0f));
    int chunkZ = static_cast<int>(floor(result.z / 16.0f));
    auto it = worldChunks.find(chunkHash(chunkX, chunkZ));
    if (it == worldChunks.end()) return;

    int localX = result.x % 16; if (localX < 0) localX += 16;
    int localZ = result.z % 16; if (localZ < 0) localZ += 16;

    it->second->blocks[localX][result.y][localZ] = BlockType::AIR;
    rebuildChunkAndBorders(result.x, result.y, result.z);
}

void placeBlock() {
    auto result = raycast(camera.Position, camera.Front, WorldConfig::INTERACTION_RANGE);
    if (!result.hit) return;

    int px = result.prevX, py = result.prevY, pz = result.prevZ;

    // Collision check con il giocatore
    float hw = camera.width / 2.0f;
    float pMinX = camera.Position.x - hw, pMaxX = camera.Position.x + hw;
    float pMinY = camera.Position.y - camera.eyeHeight, pMaxY = camera.Position.y + (camera.height - camera.eyeHeight);
    float pMinZ = camera.Position.z - hw, pMaxZ = camera.Position.z + hw;

    if (pMinX < px + 1.0f && pMaxX > px && pMinY < py + 1.0f && pMaxY > py && pMinZ < pz + 1.0f && pMaxZ > pz)
        return;

    int chunkX = static_cast<int>(floor(px / 16.0f));
    int chunkZ = static_cast<int>(floor(pz / 16.0f));
    auto it = worldChunks.find(chunkHash(chunkX, chunkZ));
    if (it == worldChunks.end()) return;

    int localX = px % 16; if (localX < 0) localX += 16;
    int localZ = pz % 16; if (localZ < 0) localZ += 16;

    if (py >= 0 && py < Chunk::HEIGHT) {
        it->second->blocks[localX][py][localZ] = placeableBlocks[selectedBlockIndex];
        rebuildChunkAndBorders(px, py, pz);
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            breakBlock();
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            placeBlock();
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    selectedBlockIndex -= static_cast<int>(yoffset);
    if (selectedBlockIndex < 0) selectedBlockIndex = PLACEABLE_COUNT - 1;
    if (selectedBlockIndex >= PLACEABLE_COUNT) selectedBlockIndex = 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime, worldChunks);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime, worldChunks);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime, worldChunks);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime, worldChunks);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessJump();
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camera.Position = glm::vec3(8.0f, 80.0f, 8.0f);
        camera.yVelocity = 0.0f;
    }
}

unsigned int loadTextureArray(const std::vector<std::string>& faces) {
    unsigned int textureArray;
    glGenTextures(1, &textureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(faces[0].c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "ERRORE: Impossibile caricare " << faces[0] << std::endl;
        return 0;
    }

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, width, height, (GLsizei)faces.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    for (unsigned int i = 0; i < faces.size(); i++) {
        data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
             std::cerr << "ERRORE: Impossibile caricare texture " << i << ": " << faces[i] << std::endl;
        }
    }

    // Genera mipmaps per ridurre aliasing a distanza
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    // NEAREST per vicino (pixel-perfect), mipmap LINEAR per distanza
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return textureArray;
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Minecraft Engine - alfanowski", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Riabilitato Culling

    Shader ourShader("../shaders/vertex.glsl", "../shaders/fragment.glsl");
    setupCrosshair();

    stbi_set_flip_vertically_on_load(true);
    std::vector<std::string> texturePaths = {
        "../assets/block/grass_block_top.png",  // 0
        "../assets/block/grass_block_side.png", // 1
        "../assets/block/dirt.png",             // 2
        "../assets/block/stone.png",            // 3
        "../assets/block/bedrock.png"           // 4
    };
    unsigned int texArray = loadTextureArray(texturePaths);

    forceLoadInitialChunks();

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        updateChunks();
        processInput(window);
        camera.UpdatePhysics(deltaTime, worldChunks);

        // Aggiorna titolo con blocco selezionato e FPS
        std::string title = "Minecraft Engine - alfanowski | Block: " + std::string(blockNames[selectedBlockIndex])
                          + " | FPS: " + std::to_string(static_cast<int>(1.0f / deltaTime));
        glfwSetWindowTitle(window, title.c_str());

        glClearColor(0.52f, 0.80f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        camera.updateFrustum((float)width / (float)height, glm::radians(45.0f), 0.1f, 500.0f);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 500.0f);
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", camera.GetViewMatrix());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
        ourShader.setInt("textureArray", 0);

        for (const auto& pair : worldChunks) {
            const auto& chunk = pair.second;

            if (chunk->isUploaded) {
                glm::vec3 min = chunk->getMin();
                glm::vec3 max = chunk->getMax();
                if (camera.frustum.isBoxVisible(min, max)) {
                    glm::mat4 model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(chunk->chunkX * 16, 0, chunk->chunkZ * 16));
                    ourShader.setMat4("model", model);
                    chunk->render();
                }
            }
        }

        glDisable(GL_DEPTH_TEST);
        drawCrosshair();
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}