#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <memory>
#include "Shader.hpp"
#include "Camera.hpp"
#include "Chunk.hpp"
#include "stb_image.h"

// --- GLOBALI ---
Camera camera(glm::vec3(8.0f, 20.0f, 30.0f));
float lastX = 640.0f, lastY = 360.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- CALLBACKS ---
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window, const std::vector<std::unique_ptr<Chunk>>& chunks) {
    // Chiudi il gioco - Esci come un pro
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Movimento con collisioni
    // Passiamo il vettore dei chunk a ogni chiamata per il controllo AABB
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime, chunks);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime, chunks);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime, chunks);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime, chunks);

    // Salto
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessJump();

    // Opzionale: un tasto per resettare la posizione se cadi nel vuoto (per la tua ansia)
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camera.Position = glm::vec3(8.0f, 20.0f, 8.0f);
        camera.yVelocity = 0.0f;
    }
}

// --- CARICAMENTO TEXTURE ARRAY ---
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

    // Alloca memoria 3D per l'array di texture
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, width, height, (GLsizei)faces.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    for (unsigned int i = 0; i < faces.size(); i++) {
        data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Minecraft", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE); // Ora che abbiamo addFace CCW, attiviamolo!

    Shader ourShader("../shaders/vertex.glsl", "../shaders/fragment.glsl");

    // --- SETUP TEXTURE ARRAY ---
    stbi_set_flip_vertically_on_load(true);
    std::vector<std::string> texturePaths = {
        "../assets/block/grass_block_top.png",  // Layer 0
        "../assets/block/grass_block_side.png", // Layer 1
        "../assets/block/dirt.png"              // Layer 2
    };
    unsigned int texArray = loadTextureArray(texturePaths);

    // --- MONDO (Griglia 4x4) ---
    std::vector<std::unique_ptr<Chunk>> worldChunks;
    for(int x = -2; x < 2; x++) {
        for(int z = -2; z < 2; z++) {
            worldChunks.push_back(std::make_unique<Chunk>(x, z));
        }
    }

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window, worldChunks);

        // Aggiornamento Fisica (Gravità)
        camera.UpdatePhysics(deltaTime, worldChunks);

        glClearColor(0.52f, 0.80f, 0.92f, 1.0f); // Sky Blue
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 500.0f);
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", camera.GetViewMatrix());

        // BINDING DEL TEXTURE ARRAY
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
        ourShader.setInt("textureArray", 0);

        for (const auto& chunk : worldChunks) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(chunk->chunkX * 16, 0, chunk->chunkZ * 16));
            ourShader.setMat4("model", model);
            chunk->render();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}