#pragma once

class shaderClass {
public:
    shaderClass(const char* vertexShaderPath, const char* fragmentShaderPath);
    void use();
private:
    unsigned int ID_program;
};