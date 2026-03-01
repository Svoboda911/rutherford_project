#pragma once
#include "libraries.h"

struct mesh {
    unsigned int VBO;
    unsigned int VAO;
    mesh();
    ~mesh();
    void upload(std::vector<float>& vertices);
};