#include <iostream>
#include <vector>

#define _USE_MATH_DEFINES
#include <cmath>

#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h"

/* 
Нарисовать просто круг (ядро)

Нарисовать движущуюся точку

Заставить точку менять направление

Потом добавить формулу силы
*/

struct particle {
    glm::vec4 position;
    glm::vec2 velocity;
};

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

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void program() {
    // glfw init -----------------------
    if(!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(940, 940, "project", NULL, NULL);
    if(!window) {glfwTerminate(); return;}; 
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return; 
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // ---------------------------------

    // vertices data -------------------
    std::vector<float> vertices = initVertices(100, 0.5);
    for (float& i : vertices) {
        std::cout << i << " " << std::endl;
    }
    // unsigned int indices[] = {};
    // ---------------------------------

    // vertices ------------------------
    unsigned int VBO;
    glGenBuffers(1, &VBO);

    unsigned int EBO;
    glGenBuffers(1, &EBO);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    // glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // ---------------------------------

    // shaders init --------------------
    shaderClass shader("../shaders/vertex.glsl", "../shaders/fragment.glsl");

    // glm -----------------------------
    unsigned int programLocModel = glGetUniformLocation(shader.ID_program, "model");
    unsigned int programLocColor = glGetUniformLocation(shader.ID_program, "color");
    glm::mat4 kernel = glm::mat4(1.0f);
    kernel = glm::translate(kernel, glm::vec3(0.0f, 0.0f, 0.0f));
    kernel = glm::scale(kernel, glm::vec3(0.2f, 0.2f, 0.2f));

    glm::vec3 kernelPos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 alphaPos = glm::vec3(-0.5f, 0.0f, 0.0f);
    glm::vec3 alphaVelocity = glm::vec3(0.3f, 0.0f, 0.0f);

    float lastTime = glfwGetTime();
    float k = 0.01f; 
    float q1 = 2.0f;
    float q2 = 79.0f;
    // ---------------------------------

    // program loop --------------------
    while(!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        glBindVertexArray(VAO);

        // physics and moves -----------------------------
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime; 
        lastTime = currentTime;

        glm::vec3 direction = kernelPos - alphaPos;
        float r = glm::length(direction);
        r = std::max(r, 0.01f);

        glm::vec3 forceDir = glm::normalize(direction);
        float forceMagnitude = k * q1 * q2 / (r*r);
        glm::vec3 force = forceDir * forceMagnitude; // a = F/m | v += a * dt | pos += v * dt

        glm::vec3 a = force / 15.0f; 
        std::cout << a.x << a.y << a.z << std::endl;
        alphaVelocity = alphaVelocity + (a * deltaTime);

        alphaPos = alphaPos + (alphaVelocity * deltaTime);
        // -----------------------------------------------

        // send uniform models ---------------------------
        glm::mat4 alpha= glm::mat4(1.0f);
        alpha = glm::translate(alpha, alphaPos);
        alpha = glm::scale(alpha, glm::vec3(0.05f));
        glUniformMatrix4fv(programLocModel, 1, GL_FALSE, glm::value_ptr(kernel));
        glUniform3f(programLocColor, 0.5f, 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size() / 3);

        glUniformMatrix4fv(programLocModel, 1, GL_FALSE, glm::value_ptr(alpha));
        glUniform3f(programLocColor, 0.9f, 0.5f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size() / 3);
        // -----------------------------------------------

        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    // ---------------------------------

    // ending program ------------------
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main() {
    program();
    return 0;
}