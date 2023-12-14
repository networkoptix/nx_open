#version 440

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 uModelViewProjectionMatrix; // offset 0
    mat3 uTexCoordMatrix; // offset 64
};

void main()
{
    fragTexCoord = (uTexCoordMatrix * vec3(aTexCoord, 1.0)).xy;
    gl_Position = uModelViewProjectionMatrix * aPosition;
}
