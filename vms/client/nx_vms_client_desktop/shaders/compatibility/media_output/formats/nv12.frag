#version 120

uniform sampler2D yTexture;
uniform sampler2D uvTexture;

vec4 textureLookup(vec2 texCoord)
{
    return vec4(texture2D(yTexture, texCoord).r,
        texture2D(uvTexture, texCoord).ra - vec2(0.5),
        1.0);
}
