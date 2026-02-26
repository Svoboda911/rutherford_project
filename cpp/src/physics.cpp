#include "physics.h"



std::vector<particles> spawnParticles(float& deltaTime, std::vector<particles>& alphas) {
    float spawnTime = 0.0f;
    float periodTime = 0.008f;
    spawnTime += deltaTime;
    if (spawnTime > periodTime) {
        spawnTime = 0.0f;
        particles alpha;
        alpha.startPos = glm::vec3(-2.0f, randomFloat(-0.9f, 0.9f), 0.0f);
        alpha.startVelocity = glm::vec3(randomFloat(0.2f, 0.25f), 0.0f, 0.0f);

        alphas.push_back(alpha);
    }
    return alphas;
} 

std::vector<float> initVertices(const int& segments, const float& radius) {
    std::vector<float> vertices = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float y = radius * sin(angle);
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(0.0f);
    }
    return vertices;
}

float randomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (max - min);
}

void updateParticle(glm::vec3& alphaPos, glm::vec3& alphaVelocity, glm::vec3& kernelPos, float deltaTime, std::vector<float>& vertices, unsigned int& programLocModel, unsigned int& programLocColor) {
    float k = 0.00015f; 
    float q1 = 2.0f;
    float q2 = 79.0f;

    glm::vec3 direction = alphaPos - kernelPos;
    float r = glm::length(direction);
    r = std::max(r, 0.01f);
        
    glm::vec3 forceDir = glm::normalize(direction);
    float forceMagnitude = k * q1 * q2 / (r*r);
    glm::vec3 force = forceDir * forceMagnitude; // a = F/m | v += a * dt | pos += v * dt

    glm::vec3 a = force / 10.0f; 

    alphaVelocity = alphaVelocity + (a * deltaTime);
    alphaPos = alphaPos + (alphaVelocity * deltaTime);

    glm::mat4 alpha= glm::mat4(1.0f);
    alpha = glm::translate(alpha, alphaPos);
    alpha = glm::scale(alpha, glm::vec3(0.02f));

    glUniformMatrix4fv(programLocModel, 1, GL_FALSE, glm::value_ptr(alpha));
    glUniform3f(programLocColor, 0.9f, 0.5f, 0.0f);
    glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size() / 3);
}
