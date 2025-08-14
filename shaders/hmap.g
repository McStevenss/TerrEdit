#version 330 core
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in vec3 vN[];
in vec3 vW[];
in vec2 vUV[];

out vec3 gsN;
out vec2 gsUV;
flat out vec3 triColor;

uniform vec3 uLightDir = normalize(vec3(0.3,1.0,0.2));

void main()
{
    // Compute per-triangle color (flat shading)
    vec3 avgNormal = normalize(vN[0] + vN[1] + vN[2]);
    vec2 avgUV = (vUV[0] + vUV[1] + vUV[2]) / 3.0;
    float ndl = max(dot(avgNormal, normalize(uLightDir)), 0.0);
    vec3 base = mix(vec3(0.15,0.35,0.15), vec3(0.5,0.4,0.3), avgUV.y);
    triColor = base * (0.2 + 0.8 * ndl);

    // Emit vertices, passing per-vertex data for smooth shading
    for(int i=0; i<3; i++)
    {
        gsN = vN[i];
        gsUV = vUV[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}