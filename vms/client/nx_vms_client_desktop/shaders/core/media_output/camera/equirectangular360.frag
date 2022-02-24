#version 330

uniform highp mat3 rotationCorrectionMatrix;

vec2 cameraProject(vec3 pointOnSphere)
{
    vec3 point = rotationCorrectionMatrix * pointOnSphere;
    return vec2(atan(point.x, point.y), asin(point.z));
}
