#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP; uniform mat4 uM;

void main()
{
    gl_Position = uVP * uM * vec4(aPos,1.0); 
}