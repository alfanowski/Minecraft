#ifndef CAMERA_H
#define CAMERA_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Chunk;

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };

// Struttura per il Frustum Culling
struct Frustum {
    struct Plane {
        glm::vec3 normal;
        float distance;

        // Normalizza il piano per calcoli corretti della distanza
        void normalize() {
            float length = glm::length(normal);
            normal /= length;
            distance /= length;
        }
    };

    Plane planes[6]; // Top, Bottom, Left, Right, Near, Far

    // Aggiorna i piani basandosi sulla matrice ViewProjection
    void update(const glm::mat4& viewProj);

    // Controlla se un blocco (o chunk) è visibile
    bool isBoxVisible(const glm::vec3& min, const glm::vec3& max) const;
};

class Camera {
public:
    glm::vec3 Position{};

    float width = 0.5f;
    float height = 1.8f;
    float eyeHeight = 1.62f;

    glm::vec3 Front;
    glm::vec3 Up{};
    glm::vec3 Right{};
    glm::vec3 WorldUp{};

    float Yaw;
    float Pitch;

    float MovementSpeed;
    float MouseSensitivity;

    float yVelocity = 0.0f;
    bool isGrounded = false;
    const float GRAVITY = -28.0f;
    const float JUMP_HEIGHT = 1.2f;

    // Frustum della camera
    Frustum frustum;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

    [[nodiscard]] glm::mat4 GetViewMatrix() const;

    void ProcessKeyboard(Camera_Movement direction, float deltaTime, const std::unordered_map<long long, std::unique_ptr<Chunk>>& chunks);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessJump();
    void UpdatePhysics(float deltaTime, const std::unordered_map<long long, std::unique_ptr<Chunk>>& chunks);

    // Aggiorna il frustum (da chiamare ogni frame prima del render)
    void updateFrustum(float aspect, float fov, float nearPlane, float farPlane);

private:
    void updateCameraVectors();
    bool checkCollision(glm::vec3 nextPos, const std::unordered_map<long long, std::unique_ptr<Chunk>>& chunks) const;
};

#endif