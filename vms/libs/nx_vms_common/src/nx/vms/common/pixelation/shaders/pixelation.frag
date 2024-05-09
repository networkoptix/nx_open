#version 440

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf
{
    mat4 mvp;
    vec2 texOffset; //< The inverse of the texture dimensions along X and Y.
    int horizontalPass; //< 0 or 1 to indicate vertical or horizontal pass.
};

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D mask;

void main()
{
    if (texture(mask, vTexCoord).a < 0.5) //< 0.0 or 1.0 is expected.
    {
        fragColor = texture(source, vTexCoord);
        return;
    }

    vec4 color = vec4(0.0);
    vec2 direction = 0 < horizontalPass ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

    vec2 off1 = vec2(1.411764705882353) * direction;
    vec2 off2 = vec2(3.2941176470588234) * direction;
    vec2 off3 = vec2(5.176470588235294) * direction;

    color += texture(source, vTexCoord) * 0.1964825501511404;
    color += texture(source, vTexCoord + (off1 * texOffset)) * 0.2969069646728344;
    color += texture(source, vTexCoord - (off1 * texOffset)) * 0.2969069646728344;
    color += texture(source, vTexCoord + (off2 * texOffset)) * 0.09447039785044732;
    color += texture(source, vTexCoord - (off2 * texOffset)) * 0.09447039785044732;
    color += texture(source, vTexCoord + (off3 * texOffset)) * 0.010381362401148057;
    color += texture(source, vTexCoord - (off3 * texOffset)) * 0.010381362401148057;

    fragColor = color;
}
