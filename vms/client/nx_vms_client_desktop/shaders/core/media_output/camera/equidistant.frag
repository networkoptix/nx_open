#version 330

vec2 cameraProject(vec3 pointOnSphere)
{
    const float kHalfPi = 3.1415926535 / 2.0;
    float theta = acos(clamp(pointOnSphere.y, -1.0, 1.0));
    return pointOnSphere.xz * (theta / (length(pointOnSphere.xz) * kHalfPi));
}
