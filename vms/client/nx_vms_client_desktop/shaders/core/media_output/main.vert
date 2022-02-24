#version 330

in highp vec4 aPosition;
in highp vec2 aTexCoord;

out highp vec2 fragTexCoord;

uniform highp mat4 uModelViewProjectionMatrix;
uniform highp mat3 uTexCoordMatrix;

void main()
{
    fragTexCoord = (uTexCoordMatrix * vec3(aTexCoord, 1.0)).xy;
    gl_Position = uModelViewProjectionMatrix * aPosition;
}
