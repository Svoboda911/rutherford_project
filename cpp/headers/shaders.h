#pragma once
#include "libraries.h"

class shader {
public:
    unsigned int program;
    shader(const char* vertexShaderPath, const char* fragmentShaderPath);
    void use();
};