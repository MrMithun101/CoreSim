#version 330 core
in vec3 v_color;
in vec3 v_world;
in vec3 v_radial;
out vec4 fragment_color;
uniform int u_finish;
uniform int u_round;
uniform mat4 u_light;
uniform vec3 u_eye;
uniform sampler2D u_depth;
const vec3 albedo[5]=vec3[5](vec3(.27,.36,.40),vec3(.21,.235,.24),vec3(.52,.245,.085),vec3(.032,.053,.060),vec3(.67,.16,.046));
const float rough[5]=float[5](.28,.92,.72,.88,.38);
float grid(vec2 p){
    vec2 d=abs(fract(p-.5)-.5)/max(fwidth(p),vec2(.0001));
    return 1.-smoothstep(.35,1.3,min(d.x,d.y));
}
float visibility(vec3 normal,vec3 sun){
    vec4 clip=u_light*vec4(v_world,1);
    vec3 p=clip.xyz/clip.w*.5+.5;
    if(any(lessThanEqual(p,vec3(0)))||any(greaterThanEqual(p,vec3(1))))return 1.;
    float bias=max(.00065,.0018*(1.-max(dot(normal,sun),0.)));
    vec2 texel=1./vec2(textureSize(u_depth,0));
    float lit=0.;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
        lit+=p.z-bias<=texture(u_depth,p.xy+vec2(x,y)*texel).r?1.:0.;
    return lit/9.;
}
void main(){
    if(u_finish==0){fragment_color=vec4(v_color,1);return;}
    int i=u_finish-1;
    vec3 n=u_round==1?normalize(v_radial):normalize(cross(dFdx(v_world),dFdy(v_world)));
    vec3 l=normalize(vec3(-16,32,20)),v=normalize(u_eye-v_world),h=normalize(l+v);
    vec3 base=albedo[i];
    if(i==1 && n.y>.9){
        float fade=1.-smoothstep(24.,65.,length(u_eye-v_world));
        base*=1.-fade*(.18*grid(v_world.xz)+.18*grid(v_world.xz/5.));
    }
    if(i==2)base*=.93+.07*sin(v_world.x*25.+2.*sin(v_world.z*3.));
    float nl=max(dot(n,l),0.), r=rough[i], metal=i==0?.65:0.;
    float gloss=pow(max(dot(n,h),0.),mix(150.,6.,r*r));
    vec3 spec=mix(vec3(.055),base,metal)*gloss*(2.-r);
    vec3 linear=base*(.21+.13*max(n.y,0.))+visibility(n,l)*nl*(base*(1.-.45*metal)+spec)*1.8;
    vec3 mapped=linear/(1.+linear);
    fragment_color=vec4(pow(mapped,vec3(1./2.2)),1);
}
