#version 330 core
in vec3 fragColor;
out vec4 diffuseColor;

void main() {
    diffuseColor = vec4(fragColor, 1.0);
}
