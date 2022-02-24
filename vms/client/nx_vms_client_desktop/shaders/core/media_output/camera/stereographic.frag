#version 330

vec2 cameraProject(vec3 pointOnSphere)
{
    return pointOnSphere.xz / (1.0 + pointOnSphere.y);
}
