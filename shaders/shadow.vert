#version 330 core
layout(location=0) in vec3 a_position;
layout(location=2) in mat4 a_transform;
uniform mat4 u_light;
void main(){gl_Position=u_light*a_transform*vec4(a_position,1.0);}
