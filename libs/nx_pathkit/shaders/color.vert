#version 440

layout(location = 0) in vec3 posAndCoverage; //< {pos.x, pos.y, coverage}

layout(location = 0) out vec4 v_color;

layout(std140, binding = 0) uniform buf
{
    mat4 mvp;
};

layout(std140, binding = 1) uniform cbuf
{
    vec4 colorData;
};

void main()
{
    const float alpha = colorData.a * posAndCoverage.z;

    v_color = vec4(
        colorData.r * alpha,
        colorData.g * alpha,
        colorData.b * alpha,
        alpha);

    gl_Position = mvp * vec4(posAndCoverage.xy, 0, 1);
}
