#version 330 core
in vec3 vN;
in vec3 vW;
in vec2 vUV;

out vec4 frag;
uniform vec3 uLightDir = normalize(vec3(0.3,1.0,0.2));
uniform vec3 uCamPos;

void main()
{
    float ndl = max(dot(normalize(vN), normalize(uLightDir)), 0.0);
    vec3 base = mix(vec3(0.15,0.35,0.15), vec3(0.5,0.4,0.3), vUV.y);
    vec3 col = base * (0.2 + 0.8*ndl);
    frag = vec4(col, 1.0);
}