#version 330

uniform highp mat3 sphereRotationMatrix;

vec3 viewUnproject(vec2 viewCoord)
{
    float cosTheta = cos(viewCoord.x);
    float sinTheta = sin(viewCoord.x);

    float cosPhi = cos(viewCoord.y);
    float sinPhi = sin(viewCoord.y);

    return vec3(cosPhi * sinTheta, cosPhi * cosTheta, sinPhi) * sphereRotationMatrix;
}
