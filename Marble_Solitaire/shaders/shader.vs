#version 330 core
layout(location = 0) in vec3 position;

uniform mat4 model;
uniform mat4 projection;
uniform vec3 color;

out vec3 fragColor;

void main() {
    gl_Position = projection * model * vec4(position, 1.0);
    fragColor = color;
}

