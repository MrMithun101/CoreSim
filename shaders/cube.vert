#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_color;
uniform mat4 u_transform;
uniform int u_finish;
uniform mat4 u_vp;
out vec3 v_color;
out vec3 v_world;
out vec3 v_radial;
void main() {
    vec4 p = u_transform * vec4(a_position,1);
    gl_Position = u_finish==0 ? p : u_vp*p;
    v_world=p.xyz;
    v_radial=mat3(u_transform)*a_position;
    v_color = a_color;
}
