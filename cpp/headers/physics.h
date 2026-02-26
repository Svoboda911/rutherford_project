#pragma once

#include <iostream>
#include <vector>

#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct particles {
    glm::vec3 startPos;
    glm::vec3 startVelocity;
};

std::vector<particles> spawnParticles(float& deltaTime, std::vector<particles>& alphas);
std::vector<float> initVertices(const int& segments, const float& radius);

float randomFloat(float min, float max);

void updateParticle(glm::vec3& alphaPos, glm::vec3& alphaVelocity, glm::vec3& kernelPos, float deltaTime, std::vector<float>& vertices, unsigned int& programLocModel, unsigned int& programLocColor);
