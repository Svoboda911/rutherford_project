#include "shaders.h"

// definition for shader class
shader::shader(const char* vertexShaderPath, const char* fragmentShaderPath) {
    std::ifstream vertexShaderFile;
    std::ifstream fragmentShaderFile;

    std::string vertexCode;
    std::string fragmentCode;
    
    vertexShaderFile.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    fragmentShaderFile.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    try {
        vertexShaderFile.open(vertexShaderPath);
        fragmentShaderFile.open(fragmentShaderPath);

        std::stringstream vertexShaderBuffer, fragmentShaderBuffer;

        vertexShaderBuffer << vertexShaderFile.rdbuf();
        fragmentShaderBuffer << fragmentShaderFile.rdbuf();

        vertexShaderFile.close();
        fragmentShaderFile.close();

        vertexCode = vertexShaderBuffer.str();
        fragmentCode = fragmentShaderBuffer.str();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    const char* vertexSourceCode = vertexCode.c_str();
    const char* fragmentSourceCode = fragmentCode.c_str();

    int success;
    char infoLog[512];

    // init vertex shader ----------------------
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSourceCode, NULL);
    glCompileShader(vertexShader);

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << infoLog << std::endl;
    }
    // -----------------------------------------

    // init fragment shader --------------------
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSourceCode, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << infoLog << std::endl;
    }
    // -----------------------------------------

    // init shader program ---------------------
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << infoLog << std::endl;
    }
    // -----------------------------------------

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void shader::use() {
    glUseProgram(program);
}