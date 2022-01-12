#version 440

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 fragColor;

// uniform block: 96 bytes
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix; // offset 0
    float qt_Opacity; // offset 64
    float fillOpacity; // offset 68
    vec2 gridStep; // offset 72
    vec4 borderColor; // offset 80
} ubuf;

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec4 colors[4];
    colors[0] = texture(source, vTexCoord);
    colors[1] = texture(source, vTexCoord - vec2(ubuf.gridStep.x, 0.0));
    colors[2] = texture(source, vTexCoord - ubuf.gridStep);
    colors[3] = texture(source, vTexCoord - vec2(0.0, ubuf.gridStep.y));

    bool same = distance(colors[0], colors[1]) + distance(colors[0], colors[2])
        + distance(colors[0], colors[3]) == 0.0;

    bool boundary = (any(lessThan(vTexCoord, ubuf.gridStep)) || any(greaterThan(vTexCoord, vec2(1.0))))
        && colors[0].a != 0.0;

    fragColor = ((same && !boundary) ? (colors[0] * ubuf.fillOpacity) : vec4(ubuf.borderColor))
        * ubuf.qt_Opacity;
}
