#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <glad.h>
#include <GLFW/glfw3.h>

class shader {
public:
    unsigned int program;
    shader(const char* vertexShaderPath, const char* fragmentShaderPath);
    void use();
};