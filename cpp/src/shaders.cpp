#include "shaders.h"
#include <filesystem>

// definition for shader class
shaderClass::shaderClass(const char* vertexShaderPath, const char* fragmentShaderPath)
    : ID_program(0) {
    std::ifstream vertexShaderFile(vertexShaderPath);
    std::ifstream fragmentShaderFile(fragmentShaderPath);

    std::string vertexCode;
    std::string fragmentCode;

    if (!vertexShaderFile.is_open() || !fragmentShaderFile.is_open()) {
        std::cerr << "[shader] Failed to open shader files.\n"
                  << "  vertex:   " << vertexShaderPath << "\n"
                  << "  fragment: " << fragmentShaderPath << "\n"
                  << "  cwd:      " << std::filesystem::current_path() << "\n";
        return;
    }

    std::stringstream vertexShaderBuffer, fragmentShaderBuffer;
    vertexShaderBuffer << vertexShaderFile.rdbuf();
    fragmentShaderBuffer << fragmentShaderFile.rdbuf();

    vertexCode = vertexShaderBuffer.str();
    fragmentCode = fragmentShaderBuffer.str();

    if (vertexCode.empty() || fragmentCode.empty()) {
        std::cerr << "[shader] Shader source is empty.\n"
                  << "  vertex size: " << vertexCode.size() << "\n"
                  << "  fragment size: " << fragmentCode.size() << "\n";
        return;
    }

    const char* vertexSourceCode = vertexCode.c_str();
    const char* fragmentSourceCode = fragmentCode.c_str();

    int success;
    char infoLog[1024];

    // init vertex shader ----------------------
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSourceCode, NULL);
    glCompileShader(vertexShader);

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertexShader, 1024, NULL, infoLog);
        std::cerr << "[shader] Vertex compile failed (" << vertexShaderPath << ")\n"
                  << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return;
    }
    // -----------------------------------------

    // init fragment shader --------------------
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSourceCode, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragmentShader, 1024, NULL, infoLog);
        std::cerr << "[shader] Fragment compile failed (" << fragmentShaderPath << ")\n"
                  << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return;
    }
    // -----------------------------------------

    // init shader program ---------------------
    ID_program = glCreateProgram();
    glAttachShader(ID_program, vertexShader);
    glAttachShader(ID_program, fragmentShader);
    glLinkProgram(ID_program);

    glGetProgramiv(ID_program, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(ID_program, 1024, NULL, infoLog);
        std::cerr << "[shader] Program link failed\n" << infoLog << std::endl;
        glDeleteProgram(ID_program);
        ID_program = 0;
    }
    // -----------------------------------------

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void shaderClass::use() {
    if (ID_program != 0) {
        glUseProgram(ID_program);
    }
}
