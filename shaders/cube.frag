#version 330 core
in vec3 v_color;
out vec4 fragment_color;
void main() {
    fragment_color = vec4(v_color, 1.0);
}
