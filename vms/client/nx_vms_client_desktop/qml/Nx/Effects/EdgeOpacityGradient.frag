#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity; //< Inherited opacity.
    vec4 thresholds;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    // Opacity per edge proximity, left-top-right-bottom.
    mediump vec4 opacities = vec4(1.0) - max((vec4(1.0) - vec4(qt_TexCoord0, vec2(1.0)
        - qt_TexCoord0) / thresholds), vec4(0.0));

    // Final opacity is a product of all four.
    mediump float opacity = opacities.s * opacities.t * opacities.p * opacities.q;

    lowp vec4 rgba = texture(source, qt_TexCoord0);
    fragColor = rgba * (qt_Opacity * opacity);
}
