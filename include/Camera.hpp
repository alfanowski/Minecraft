#ifndef CAMERA_H
#define CAMERA_H

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Chunk;

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };

class Camera {
public:
    // Attributi camera
    glm::vec3 Position{};

    // Dimensioni Player (Minecraft style)
    float width = 0.5f;
    float height = 1.8f;      // Altezza totale hitbox
    float eyeHeight = 1.62f;  // Altezza occhi da terra

    glm::vec3 Front;
    glm::vec3 Up{};
    glm::vec3 Right{};
    glm::vec3 WorldUp{};

    // Angoli di Eulero
    float Yaw;
    float Pitch;

    // Opzioni camera
    float MovementSpeed;
    float MouseSensitivity;

    // Fisica
    float yVelocity = 0.0f;
    bool isGrounded = false;
    const float GRAVITY = -28.0f;
    const float JUMP_HEIGHT = 1.2f;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

    [[nodiscard]] glm::mat4 GetViewMatrix() const;

    void ProcessKeyboard(Camera_Movement direction, float deltaTime, const std::vector<std::unique_ptr<Chunk>>& chunks);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessJump();
    void UpdatePhysics(float deltaTime, const std::vector<std::unique_ptr<Chunk>>& chunks);

private:
    void updateCameraVectors();
    bool checkCollision(glm::vec3 nextPos, const std::vector<std::unique_ptr<Chunk>>& chunks) const;
};

#endif