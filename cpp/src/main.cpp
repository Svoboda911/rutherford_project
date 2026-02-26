#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

#define _USE_MATH_DEFINES
#include <cmath>

#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders.h"
#include "physics.h"

void framebuffer_size_callback(GLFWwindow* window, int& width, int& height, glm::mat4& projection) {
    glViewport(0, 0, width, height);
    float aspect = (float)width / (float)height;
    projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f);
}

void program() {
    // glfw init -----------------------
    if(!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    int width = 1540;
    int height = 840;
    glm::mat4 projection;
    GLFWwindow* window = glfwCreateWindow(width, height, "project", NULL, NULL);
    if(!window) {glfwTerminate(); return;}; 
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return; 
    glfwGetFramebufferSize(window, &width, &height);
    framebuffer_size_callback(window, width, height, projection);
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
    shader shader("../shaders/vertex.glsl", "../shaders/fragment.glsl");

    // glm -----------------------------
    glm::mat4 kernel = glm::mat4(1.0f);
    kernel = glm::translate(kernel, glm::vec3(0.55f, 0.0f, 0.0f));
    kernel = glm::scale(kernel, glm::vec3(0.08f));

    glm::vec3 kernelPos = glm::vec3(0.55f, 0.0f, 0.0f);
    // ---------------------------------

    std::vector<particles> alphas;

    // program loop --------------------
    unsigned int programLocModel = glGetUniformLocation(shader.program, "model");
    unsigned int programLocColor = glGetUniformLocation(shader.program, "color");
    unsigned int programLocProjection = glGetUniformLocation(shader.program, "projection");
    float lastTime = glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        glBindVertexArray(VAO);

        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime; 
        lastTime = currentTime;

        // send uniform models ---------------------------
        glUniformMatrix4fv(programLocProjection, 1, GL_FALSE, glm::value_ptr(projection));

        glUniformMatrix4fv(programLocModel, 1, GL_FALSE, glm::value_ptr(kernel));
        glUniform3f(programLocColor, 0.5f, 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size() / 3);
    
        spawnParticles(deltaTime, alphas);
        for(auto& alpha : alphas) {
            updateParticle(alpha.startPos, alpha.startVelocity, kernelPos, deltaTime, vertices, programLocModel, programLocColor);
        }
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