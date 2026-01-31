#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include "Shader.hpp"
#include "Camera.hpp"
#include "Chunk.hpp"

// Non definire STB_IMAGE_IMPLEMENTATION qui se lo hai già fatto in stb_setup.cpp!
#include "stb_image.h"

// --- GLOBALI ---
// Iniziamo la camera un po' distante dal centro del chunk (8, 16, 8) per vederlo bene
Camera camera(glm::vec3(8.0f, 15.0f, 30.0f));
float lastX = 640.0f, lastY = 360.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// --- CALLBACKS ---
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    auto xpos = static_cast<float>(xposIn);
    auto ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
}

int main() {
    // --- INIT GLFW ---
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Necessario su Mac

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Minecraft Engine - alfanowski", nullptr, nullptr);
    if (!window) {
        std::cerr << "Errore creazione finestra" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // --- OPENGL STATE ---
    glEnable(GL_DEPTH_TEST);
    // Culling delle facce posteriori per performance (opzionale ma consigliato)
    //glEnable(GL_CULL_FACE);

    // --- SHADER & TEXTURE ---
    Shader ourShader("../shaders/vertex.glsl", "../shaders/fragment.glsl");

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Minecraft style: niente sfocature, solo pixel duri e puri
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    // Assicurati che il path sia corretto rispetto all'eseguibile!
    unsigned char *data = stbi_load("../assets/dirt.png", &width, &height, &nrChannels, 0);

    if (data) {
        const GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "ERRORE: Impossibile caricare la texture in ../assets/dirt.png" << std::endl;
    }
    stbi_image_free(data);

    // --- CHUNK ---
    Chunk myChunk;

    // --- LOOP ---
    while (!glfwWindowShouldClose(window)) {
        auto currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Colore del cielo (Celeste chiaro)
        glClearColor(0.52f, 0.80f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        // Matrici per la trasformazione 3D
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(fbWidth) / static_cast<float>(fbHeight), 0.1f, 200.0f);
        glm::mat4 view = camera.GetViewMatrix();
        auto model = glm::mat4(1.0f);

        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setMat4("model", model);

        // Binding della texture e render
        glBindTexture(GL_TEXTURE_2D, texture);
        myChunk.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}