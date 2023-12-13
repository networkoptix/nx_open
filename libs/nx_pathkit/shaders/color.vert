#version 440

layout(location = 0) in vec3 posAlpha;

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
    v_color = vec4(
        colorData.r * colorData.a,
        colorData.g * colorData.a,
        colorData.b * colorData.a,
        posAlpha.z * colorData.a);

    gl_Position = mvp * vec4(posAlpha.xy, 0, 1);
}
