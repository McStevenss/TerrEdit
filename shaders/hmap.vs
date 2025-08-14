#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNrmM;
out vec3 vN;
out vec3 vW;
out vec2 vUV;
void main()
{
    vec4 wpos = uModel * vec4(aPos,1.0);
    vW = wpos.xyz;
    vN = normalize(uNrmM * aNrm);
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos,1.0);
}