#include "libraries.h"
#include "shaders.h"
#include "physics.h"
#include "vertices.h"
#include "geometry.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void program() {
    // glfw init -----------------------
    if(!glfwInit()) return;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    int width = 1540;
    int height = 840;
    GLFWwindow* window = glfwCreateWindow(width, height, "project", NULL, NULL);
    if(!window) {glfwTerminate(); return;}; 
    glfwMakeContextCurrent(window);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return; 
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    // ---------------------------------


 
    shader shader("../shaders/vertex.glsl", "../shaders/fragment.glsl"); // shader and program initinalisation



    // vertices data -------------------
    mesh circle;
    auto vertices = geometry::circle(100, 0.5f);
    circle.upload(vertices);
    // ---------------------------------



    // glm -----------------------------
    // objects
    particles kernel;
    kernel.model = glm::mat4(1.0f);
    kernel.position = glm::vec3(0.55f, 0.0f, 0.0f);
    kernel.scale = glm::vec3(0.08f);

    particles alpha;
    alpha.model = glm::mat4(1.0f);
    alpha.position = glm::vec3(-2.0f, randomFloat(-0.9f, 0.9f), 0.0f);
    alpha.scale = glm::vec3(0.02f);
    alpha.velocity = glm::vec3(randomFloat(0.2f, 0.25f), 0.0f, 0.0f);

    std::vector<particles> alphas;
    
    glm::mat4 projection = glm::mat4(1.0f);
    float aspect = (float)width / (float)height;

    // operations
    kernel.model = glm::translate(kernel.model, kernel.position);
    kernel.model = glm::scale(kernel.model, kernel.scale);
        
    projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f);
    // ---------------------------------


    // program loop --------------------
    unsigned int programLocModel = glGetUniformLocation(shader.program, "model");
    unsigned int programLocColor = glGetUniformLocation(shader.program, "color");
    unsigned int programLocProjection = glGetUniformLocation(shader.program, "projection");
    float lastTime = glfwGetTime();
    while(!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        float deltaTime = getDeltaTime(lastTime);

        glBindVertexArray(circle.VAO);

        glUniformMatrix4fv(programLocProjection, 1, GL_FALSE, glm::value_ptr(projection));

        glUniformMatrix4fv(programLocModel, 1, GL_FALSE, glm::value_ptr(kernel.model));
        glUniform3f(programLocColor, 0.5f, 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size()/3);

        spawnParticles(alpha, deltaTime, alphas);
        for(auto& p : alphas) {
            updateParticle(p, kernel, deltaTime);
            glUniformMatrix4fv(programLocModel, 1, GL_FALSE, glm::value_ptr(p.model));
            glUniform3f(programLocColor, 0.9f, 0.5f, 0.0f);
            glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size()/3);
        }

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