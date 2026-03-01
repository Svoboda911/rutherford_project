#include "physics.h"

float randomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (max - min);
}
float getDeltaTime(float& lastTime) {
    float currentTime = glfwGetTime();
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;
    return deltaTime;
}



void spawnParticles(particles& alpha, float& deltaTime, std::vector<particles>& alphas) {
    float static spawnTime = 0.0f;
    float periodTime = 0.35f;
    spawnTime += deltaTime;
    if (spawnTime > periodTime) {
        spawnTime = 0.0f;
        alpha.position = glm::vec3(-2.0f, randomFloat(-0.9f, 0.9f), 0.0f);
        alpha.velocity = glm::vec3(randomFloat(0.25f, 0.30f), 0.0f, 0.0f);

        alphas.push_back(alpha);
    }
} 

void updateParticle(particles& alpha, const particles& kernel, float deltaTime) {
    float k = 0.0002f; 
    float q1 = 2.0f;
    float q2 = 79.0f;

    glm::vec3 direction = alpha.position - kernel.position;
    float r = glm::length(direction);
    r = std::max(r, 0.01f);
        
    glm::vec3 forceDir = glm::normalize(direction);
    float forceMagnitude = k * q1 * q2 / (r*r);
    glm::vec3 force = forceDir * forceMagnitude; // a = F/m | v += a * dt | pos += v * dt

    glm::vec3 a = force / 10.0f; 

    alpha.velocity = alpha.velocity + (a * deltaTime);
    alpha.position = alpha.position + (alpha.velocity * deltaTime);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, alpha.position);
    model = glm::scale(model, alpha.scale);

    alpha.model = model;
}
