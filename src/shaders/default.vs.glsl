R"|(

#version 330 core
layout (location = 0) in vec3 vertP;
// TODO Should be an attribute at some point
uniform mat4 modelM;
uniform mat4 projM;

void main()
{
    gl_Position = projM * modelM * vec4(vertP, 1.0);
}

)|"

