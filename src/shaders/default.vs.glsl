R"|(

#version 330 core
layout (location = 0) in vec3 pVertex;
// TODO Should be an attribute at some point
uniform mat4 mTransform;

void main()
{
    gl_Position = mTransform * vec4(pVertex, 1.0);
}

)|"

