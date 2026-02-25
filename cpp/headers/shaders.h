#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <glad.h>
#include <GLFW/glfw3.h>

class shaderClass {
public:
    unsigned int ID_program;
    shaderClass(const char* vertexShaderPath, const char* fragmentShaderPath);
    void use();
};