#include <iostream>

#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "shaders.h"

void program() {
    // glfw init -----------------------
    if(!glfwInit) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(512, 512, "project", NULL, NULL);
    if(!window) { glfwTerminate; return;}; 
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return; 
    glViewport(0, 0, 512, 512);
    // ---------------------------------

    // vertices data -------------------
    float vertices[] = {};
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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // ---------------------------------

    // shaders init --------------------
    shaderClass shader("../shaders/vertex.glsl", "../shaders/fragment.glsl");

    // program loop --------------------
    while(!glfwWindowShouldClose(window)) {

        shader.use();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    // ---------------------------------

    // ending program ------------------
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main() {

    return 0;
}