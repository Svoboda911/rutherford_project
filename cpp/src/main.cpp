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

void program() {
    // glfw init -----------------------
    if(!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(512, 512, "project", NULL, NULL);
    if(!window) {glfwTerminate(); return;}; 
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return; 
    glViewport(0, 0, 512, 512);
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
    unsigned int programLoc = glGetUniformLocation(shader.ID_program, "model");
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));

    glm::mat4 model2 = glm::mat4(1.0f);
    model2 = glm::translate(model2, glm::vec3(-0.5f, 0.0f, 0.0f));
    // ---------------------------------

    // program loop --------------------
    while(!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 0.1f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader.use();

        glUniformMatrix4fv(programLoc, 1, GL_FALSE, glm::value_ptr(model2));
        glUniformMatrix4fv(programLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size() / 3);
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