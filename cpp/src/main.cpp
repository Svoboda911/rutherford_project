#include <iostream>

#include <glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

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
    // ---------------------------------

    // shaders init --------------------

    // program loop --------------------
    while(!glfwWindowShouldClose(window)) {

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