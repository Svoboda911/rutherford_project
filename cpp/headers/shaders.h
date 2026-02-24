#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <glad.h>
#include <GLFW/glfw3.h>

class shaderClass {
public:
    shaderClass(const char* vertexShaderPath, const char* fragmentShaderPath);
    void use();
private:
    unsigned int ID_program;
};