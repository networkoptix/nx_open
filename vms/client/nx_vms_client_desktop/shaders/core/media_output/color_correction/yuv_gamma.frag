#version 330

uniform mediump float gamma;
uniform mediump float luminocityShift;
uniform mediump float luminocityFactor;

vec4 colorCorrection(vec4 source)
{
    return vec4(
        clamp(pow(max(source.r + luminocityShift, 0.0) * luminocityFactor, gamma), 0.0, 1.0),
        source.gba);
}
