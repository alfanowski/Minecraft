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
#include "Shader.hpp"
#include "Camera.hpp"
#include "Chunk.hpp"
#include "stb_image.h"

// --- GLOBALI ---
Camera camera(glm::vec3(8.0f, 80.0f, 30.0f)); // Camera ripristinata
std::unordered_map<long long, std::unique_ptr<Chunk>> worldChunks;

struct PendingChunk {
    long long key;
    int x, z;
    std::future<void> task;
};
std::list<PendingChunk> generationQueue;
std::vector<Chunk*> uploadQueue;

float lastX = 640.0f, lastY = 360.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

const int RENDER_DISTANCE = 8;

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

// --- GESTIONE MONDO ASINCRONA ---
void updateChunks() {
    int playerChunkX = static_cast<int>(floor(camera.Position.x / 16.0f));
    int playerChunkZ = static_cast<int>(floor(camera.Position.z / 16.0f));

    for (int x = playerChunkX - RENDER_DISTANCE; x <= playerChunkX + RENDER_DISTANCE; x++) {
        for (int z = playerChunkZ - RENDER_DISTANCE; z <= playerChunkZ + RENDER_DISTANCE; z++) {
            long long key = chunkHash(x, z);

            bool alreadyQueued = false;
            for(const auto& pc : generationQueue) { if(pc.key == key) { alreadyQueued = true; break; } }

            if (worldChunks.find(key) == worldChunks.end() && !alreadyQueued) {
                worldChunks[key] = std::make_unique<Chunk>(x, z);
                Chunk* chunkPtr = worldChunks[key].get();

                generationQueue.push_back({
                    key, x, z,
                    std::async(std::launch::async, [chunkPtr]() {
                        chunkPtr->generate();
                    })
                });
            }
        }
    }

    for (auto it = generationQueue.begin(); it != generationQueue.end(); ) {
        if (it->task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            long long key = it->key;
            if (worldChunks.find(key) != worldChunks.end()) {
                uploadQueue.push_back(worldChunks[key].get());
            }
            it = generationQueue.erase(it);
        } else {
            ++it;
        }
    }

    int uploadsPerFrame = 16;
    for (int i = 0; i < uploadsPerFrame && !uploadQueue.empty(); i++) {
        Chunk* chunk = uploadQueue.back();
        uploadQueue.pop_back();
        chunk->upload();
    }

    for (auto it = worldChunks.begin(); it != worldChunks.end(); ) {
        int cx = it->second->chunkX;
        int cz = it->second->chunkZ;
        if (abs(cx - playerChunkX) > RENDER_DISTANCE + 2 ||
            abs(cz - playerChunkZ) > RENDER_DISTANCE + 2) {
            it = worldChunks.erase(it);
        } else {
            ++it;
        }
    }
}

void forceLoadInitialChunks() {
    int playerChunkX = static_cast<int>(floor(camera.Position.x / 16.0f));
    int playerChunkZ = static_cast<int>(floor(camera.Position.z / 16.0f));
    int initialRadius = 2;

    for (int x = playerChunkX - initialRadius; x <= playerChunkX + initialRadius; x++) {
        for (int z = playerChunkZ - initialRadius; z <= playerChunkZ + initialRadius; z++) {
            long long key = chunkHash(x, z);
            if (worldChunks.find(key) == worldChunks.end()) {
                worldChunks[key] = std::make_unique<Chunk>(x, z);
                worldChunks[key]->generate();
                worldChunks[key]->upload();
            }
        }
    }
}

void breakBlock() {
    glm::vec3 rayOrigin = camera.Position;
    glm::vec3 rayDir = camera.Front;
    float maxDist = 5.0f;
    float step = 0.05f;

    for (float t = 0.0f; t < maxDist; t += step) {
        glm::vec3 p = rayOrigin + rayDir * t;
        int x = static_cast<int>(floor(p.x));
        int y = static_cast<int>(floor(p.y));
        int z = static_cast<int>(floor(p.z));
        int chunkX = static_cast<int>(floor(x / 16.0f));
        int chunkZ = static_cast<int>(floor(z / 16.0f));
        long long key = chunkHash(chunkX, chunkZ);
        auto it = worldChunks.find(key);

        if (it != worldChunks.end()) {
            int localX = x % 16; if (localX < 0) localX += 16;
            int localZ = z % 16; if (localZ < 0) localZ += 16;
            if (y >= 0 && y < Chunk::HEIGHT) {
                if (it->second->blocks[localX][y][localZ] != 0) {
                    it->second->blocks[localX][y][localZ] = 0;

                    // FIX: Usa rebuild() per aggiornare correttamente la GPU
                    it->second->rebuild();
                    return;
                }
            }
        }
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
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        breakBlock();
    }
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

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
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