#version 330 core
in vec3 v_color;
out vec4 fragment_color;
uniform int u_finish;
const vec3 palette[5]=vec3[5](vec3(.42,.52,.60),vec3(.38,.4,.42),vec3(.72,.49,.27),vec3(.14,.21,.23),vec3(.84,.39,.18));
void main() {
    fragment_color = vec4(u_finish==0 ? v_color : palette[u_finish-1]*(.55+.55*v_color.g),1.0);
}
