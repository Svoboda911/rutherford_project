#pragma once
#include "libraries.h"

struct particles {
    glm::mat4 model;
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 scale;
};

float getDeltaTime(float& lastTime);
float randomFloat(float min, float max);

void spawnParticles(particles& alpha, float& deltaTime, std::vector<particles>& alphas);
void updateParticle(particles& alpha, const particles& kernel, float deltaTime);