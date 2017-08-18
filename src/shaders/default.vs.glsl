R"|(

#version 330 core
in vec3 pIn;
in vec2 uvIn;
in vec4 cIn;

uniform mat4 mTransform;

void main()
{
    gl_Position = mTransform * vec4(pIn, 1.0);
}

)|"

