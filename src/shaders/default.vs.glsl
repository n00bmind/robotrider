R"|(

#version 330 core
layout (location = 0) in vec3 vertP;
uniform mat4 projM;

void main()
{
    gl_Position = projM * vec4(vertP, 1.0);
}

)|"

