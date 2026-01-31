#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <glm/glm.hpp> // Serve per mat4

class Shader {
public:
    unsigned int ID;
    Shader(const char* vertexPath, const char* fragmentPath);
    void use();

    // Metodi esistenti...
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;

    // AGGIUNGI QUESTO:
    void setMat4(const std::string &name, const glm::mat4 &mat) const;
};
#endif