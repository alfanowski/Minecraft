#include "Camera.hpp"
#include "Chunk.hpp"
#include <cmath>
#include <algorithm>

// --- IMPLEMENTAZIONE FRUSTUM ---
void Frustum::update(const glm::mat4& viewProj) {
    // Estrazione dei piani dalla matrice ViewProjection (Gribb/Hartmann method)
    // Left
    planes[0].normal.x = viewProj[0][3] + viewProj[0][0];
    planes[0].normal.y = viewProj[1][3] + viewProj[1][0];
    planes[0].normal.z = viewProj[2][3] + viewProj[2][0];
    planes[0].distance = viewProj[3][3] + viewProj[3][0];
    planes[0].normalize();

    // Right
    planes[1].normal.x = viewProj[0][3] - viewProj[0][0];
    planes[1].normal.y = viewProj[1][3] - viewProj[1][0];
    planes[1].normal.z = viewProj[2][3] - viewProj[2][0];
    planes[1].distance = viewProj[3][3] - viewProj[3][0];
    planes[1].normalize();

    // Bottom
    planes[2].normal.x = viewProj[0][3] + viewProj[0][1];
    planes[2].normal.y = viewProj[1][3] + viewProj[1][1];
    planes[2].normal.z = viewProj[2][3] + viewProj[2][1];
    planes[2].distance = viewProj[3][3] + viewProj[3][1];
    planes[2].normalize();

    // Top
    planes[3].normal.x = viewProj[0][3] - viewProj[0][1];
    planes[3].normal.y = viewProj[1][3] - viewProj[1][1];
    planes[3].normal.z = viewProj[2][3] - viewProj[2][1];
    planes[3].distance = viewProj[3][3] - viewProj[3][1];
    planes[3].normalize();

    // Near
    planes[4].normal.x = viewProj[0][3] + viewProj[0][2];
    planes[4].normal.y = viewProj[1][3] + viewProj[1][2];
    planes[4].normal.z = viewProj[2][3] + viewProj[2][2];
    planes[4].distance = viewProj[3][3] + viewProj[3][2];
    planes[4].normalize();

    // Far
    planes[5].normal.x = viewProj[0][3] - viewProj[0][2];
    planes[5].normal.y = viewProj[1][3] - viewProj[1][2];
    planes[5].normal.z = viewProj[2][3] - viewProj[2][2];
    planes[5].distance = viewProj[3][3] - viewProj[3][2];
    planes[5].normalize();
}

bool Frustum::isBoxVisible(const glm::vec3& min, const glm::vec3& max) const {
    // Controlla se un AABB è dentro o interseca il frustum
    for (int i = 0; i < 6; i++) {
        glm::vec3 p = min;
        if (planes[i].normal.x >= 0) p.x = max.x;
        if (planes[i].normal.y >= 0) p.y = max.y;
        if (planes[i].normal.z >= 0) p.z = max.z;

        // Se il punto positivo è dietro il piano, l'intera scatola è fuori
        if (glm::dot(planes[i].normal, p) + planes[i].distance < 0) {
            return false;
        }
    }
    return true;
}

// --- IMPLEMENTAZIONE CAMERA ---

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(4.3f), MouseSensitivity(0.1f) {
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

void Camera::updateFrustum(float aspect, float fov, float nearPlane, float farPlane) {
    glm::mat4 proj = glm::perspective(fov, aspect, nearPlane, farPlane);
    glm::mat4 view = GetViewMatrix();
    frustum.update(proj * view);
}

bool Camera::checkCollision(glm::vec3 nextPos, const std::unordered_map<long long, std::unique_ptr<Chunk>>& chunks) const {
    float halfWidth = width / 2.0f;
    float minX = nextPos.x - halfWidth;
    float maxX = nextPos.x + halfWidth;
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

                long long key = chunkHash(chunkX, chunkZ);
                auto it = chunks.find(key);

                if (it != chunks.end()) {
                    const auto& chunk = it->second;
                    int localX = x % 16; if (localX < 0) localX += 16;
                    int localZ = z % 16; if (localZ < 0) localZ += 16;

                    if (y >= 0 && y < Chunk::HEIGHT) {
                        if (chunk->blocks[localX][y][localZ] != 0) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime, const std::unordered_map<long long, std::unique_ptr<Chunk>>& chunks) {
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
        if (!checkCollision(nextPos, chunks)) Position.x = nextPos.x;
        nextPos = Position;
        nextPos.z += movement.z;
        if (!checkCollision(nextPos, chunks)) Position.z = nextPos.z;
    }
}

void Camera::ProcessJump() {
    if (isGrounded) {
        yVelocity = std::sqrt(2.0f * std::abs(GRAVITY) * JUMP_HEIGHT);
        isGrounded = false;
    }
}

void Camera::UpdatePhysics(float deltaTime, const std::unordered_map<long long, std::unique_ptr<Chunk>>& chunks) {
    yVelocity += GRAVITY * deltaTime;
    if (yVelocity < -50.0f) yVelocity = -50.0f;

    float dy = yVelocity * deltaTime;
    glm::vec3 nextPos = Position;
    nextPos.y += dy;

    if (checkCollision(nextPos, chunks)) {
        if (yVelocity < 0) {
            isGrounded = true;
            yVelocity = 0.0f;
        } else {
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
