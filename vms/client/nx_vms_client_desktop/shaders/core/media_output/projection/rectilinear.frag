#version 330

uniform highp mat3 unprojectionMatrix;

vec3 viewUnproject(vec2 viewCoord)
{
    return unprojectionMatrix * vec3(viewCoord, 1.0);
}
