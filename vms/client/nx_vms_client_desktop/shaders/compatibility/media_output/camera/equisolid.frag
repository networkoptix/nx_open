#version 120

vec2 cameraProject(vec3 pointOnSphere)
{
    return pointOnSphere.xz / sqrt(1.0 + pointOnSphere.y);
}
