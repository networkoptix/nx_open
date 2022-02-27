#version 120

attribute highp vec4 aPosition;
attribute highp vec2 aTexCoord;

varying highp vec2 fragTexCoord;

uniform highp mat4 uModelViewProjectionMatrix;
uniform highp mat3 uTexCoordMatrix;

void main()
{
    fragTexCoord = (uTexCoordMatrix * vec3(aTexCoord, 1.0)).xy;
    gl_Position = uModelViewProjectionMatrix * aPosition;
}
