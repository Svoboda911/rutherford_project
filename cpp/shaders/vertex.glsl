#version 330 core
layout (location = 0) in vec3 aPos;
out vec4 ourColor;

uniform mat4 model;
uniform vec3 color;

void main() {
    gl_Position = model * vec4(aPos, 1.0f);
    ourColor = vec4(color, 1.0f);
}