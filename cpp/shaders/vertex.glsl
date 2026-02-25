#version 330 core
layout (location = 0) in vec3 aPos;
out vec4 ourColor;

uniform mat4 model;

void main() {
    gl_Position = model * vec4(aPos, 1.0f);
    ourColor = vec4(0.0f, 0.5f, 0.5f, 1.0f);
}