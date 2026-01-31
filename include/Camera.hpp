#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };

class Camera {
public:
    // Attributi camera
    glm::vec3 Position{};
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

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

    // Ritorna la matrice di vista (quella che passeremo allo shader)
    [[nodiscard]] glm::mat4 GetViewMatrix() const;

    // Elabora l'input della tastiera
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);

    // Elabora l'input del mouse
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

private:
    void updateCameraVectors();
};

#endif