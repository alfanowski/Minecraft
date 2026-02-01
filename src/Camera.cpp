#include "Camera.hpp"
#include "Chunk.hpp"
#include <cmath>
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(4.3f), MouseSensitivity(0.1f) {
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    // Position è già la posizione degli occhi
    return glm::lookAt(Position, Position + Front, Up);
}

bool Camera::checkCollision(glm::vec3 nextPos, const std::vector<std::unique_ptr<Chunk>>& chunks) const {
    // Bounding Box del player relativa alla posizione degli OCCHI (nextPos)
    float halfWidth = width / 2.0f;

    float minX = nextPos.x - halfWidth;
    float maxX = nextPos.x + halfWidth;

    // Calcolo Y basato sull'altezza degli occhi
    float feetY = nextPos.y - eyeHeight;
    float headY = nextPos.y + (height - eyeHeight);

    float minY = feetY;
    float maxY = headY;

    float minZ = nextPos.z - halfWidth;
    float maxZ = nextPos.z + halfWidth;

    int startX = static_cast<int>(floor(minX));
    int endX   = static_cast<int>(floor(maxX));
    int startY = static_cast<int>(floor(minY));
    int endY   = static_cast<int>(floor(maxY));
    int startZ = static_cast<int>(floor(minZ));
    int endZ   = static_cast<int>(floor(maxZ));

    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            for (int z = startZ; z <= endZ; z++) {

                int chunkX = static_cast<int>(floor(x / 16.0f));
                int chunkZ = static_cast<int>(floor(z / 16.0f));

                int localX = x % 16; if (localX < 0) localX += 16;
                int localZ = z % 16; if (localZ < 0) localZ += 16;

                for (const auto& chunk : chunks) {
                    if (chunk->chunkX == chunkX && chunk->chunkZ == chunkZ) {
                        if (y >= 0 && y < Chunk::HEIGHT) {
                            if (chunk->blocks[localX][y][localZ] != 0) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime, const std::vector<std::unique_ptr<Chunk>>& chunks) {
    float velocity = MovementSpeed * deltaTime;

    glm::vec3 forward = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
    glm::vec3 right = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

    glm::vec3 movement(0.0f);
    if (direction == FORWARD)  movement += forward;
    if (direction == BACKWARD) movement -= forward;
    if (direction == LEFT)     movement -= right;
    if (direction == RIGHT)    movement += right;

    if (glm::length(movement) > 0.0f) {
        movement = glm::normalize(movement) * velocity;

        glm::vec3 nextPos = Position;
        nextPos.x += movement.x;
        if (!checkCollision(nextPos, chunks)) {
            Position.x = nextPos.x;
        }

        nextPos = Position;
        nextPos.z += movement.z;
        if (!checkCollision(nextPos, chunks)) {
            Position.z = nextPos.z;
        }
    }
}

void Camera::ProcessJump() {
    if (isGrounded) {
        yVelocity = std::sqrt(2.0f * std::abs(GRAVITY) * JUMP_HEIGHT);
        isGrounded = false;
    }
}

void Camera::UpdatePhysics(float deltaTime, const std::vector<std::unique_ptr<Chunk>>& chunks) {
    yVelocity += GRAVITY * deltaTime;
    if (yVelocity < -50.0f) yVelocity = -50.0f;

    float dy = yVelocity * deltaTime;
    glm::vec3 nextPos = Position;
    nextPos.y += dy;

    if (checkCollision(nextPos, chunks)) {
        if (yVelocity < 0) {
            // Atterraggio
            isGrounded = true;
            yVelocity = 0.0f;

            // Snap preciso al blocco sottostante
            // Calcoliamo dove dovrebbero essere i piedi
            float currentFeetY = Position.y - eyeHeight;
            float targetFeetY = floor(currentFeetY); // Assumiamo che il blocco sia a coordinate intere

            // Se eravamo sopra un blocco (o molto vicini), allineiamoci
            // Questo è un fix semplice: se stiamo cadendo e tocchiamo qualcosa,
            // probabilmente è il pavimento a floor(y).
            // Ma attenzione: checkCollision ha rilevato collisione a nextPos.
            // Proviamo a trovare l'altezza intera più vicina sotto i piedi.

            // Metodo semplice: annulla il movimento verticale che ha causato la compenetrazione
            // E magari arrotonda leggermente se siamo molto vicini a un intero

            // Per ora manteniamo la logica semplice: non muoverti se c'è collisione
            // Ma per evitare di rimanere "appesi" a 0.001mm dal suolo, potremmo forzare
            // la posizione se la distanza è minima.

        } else {
            // Testata
            yVelocity = 0.0f;
        }
    } else {
        Position.y = nextPos.y;
        isGrounded = false;
    }
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    if (constrainPitch) {
        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;
    }
    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);

    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}
