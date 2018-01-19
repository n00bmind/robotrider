R"|(

#version 330 core

layout(location = 0) in vec3 pIn;
layout(location = 1) in vec2 uvIn;
layout(location = 2) in uint cIn;

flat out uint vertColor;

uniform mat4 mTransform;

void main()
{
    gl_Position = mTransform * vec4(pIn, 1.0);
    vertColor = cIn;
}

)|"

