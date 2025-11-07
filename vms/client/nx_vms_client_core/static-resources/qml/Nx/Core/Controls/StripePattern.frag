#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    float stripeWidth;
    float gapWidth;
    vec4 color;
    vec4 backgroundColor;
    vec2 itemSize;
    vec2 uvOffset;
    vec2 normal;
};

float symmetricMod(float x, float m)
{
    return x - m * floor(x / m + 0.5);
}

void main()
{
    vec2 uv = (qt_TexCoord0 * itemSize) + uvOffset; //< De-normalized texture coords with offset.
    float positionAlongNormal = dot(uv, normal);

    float period = stripeWidth + gapWidth;
    float halfStripe = stripeWidth * 0.5;

    // Signed position normalized in [-period/2, period/2] with 0 in the middle of a stripe.
    float normalizedPosition = symmetricMod(positionAlongNormal - halfStripe, period);

    // Position relative to the stripe's edge: negative inside the stripe, positive outside.
    float positionRelativeToEdge = abs(normalizedPosition) - halfStripe;

    // Compute blending weight for antialiasing.
    // 0 if farther than half a fragment inside the stripe,
    // 1 if farther than half a fragment outside the stripe,
    // smoothly interpolated in between.
    float halfFragmentSize = fwidth(positionAlongNormal) * 0.5;
    float weight = smoothstep(-halfFragmentSize, halfFragmentSize, positionRelativeToEdge);

    fragColor = mix(color, backgroundColor, weight) * qt_Opacity;
}
