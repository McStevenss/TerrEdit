#version 330 core
in vec3 gsN;
in vec2 gsUV;
flat in vec3 triColor;

out vec4 frag;

uniform vec3 uLightDir = normalize(vec3(0.3,1.0,0.2));
uniform bool uFlatShading = false;

void main()
{
    if(uFlatShading)
    {
        frag = vec4(triColor,1.0);
    }
    else
    {
        float ndl = max(dot(normalize(gsN), normalize(uLightDir)), 0.0);
        vec3 base = mix(vec3(0.15,0.35,0.15), vec3(0.5,0.4,0.3), gsUV.y);
        vec3 col = base * (0.2 + 0.8*ndl);
        frag = vec4(col,1.0);
    }
}