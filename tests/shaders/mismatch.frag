#version 330 core
in vec2 v_color;
out vec4 fragment_color;
void main() { fragment_color = vec4(v_color, 0.0, 1.0); }
