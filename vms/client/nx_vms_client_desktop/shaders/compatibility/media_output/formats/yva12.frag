#version 120

uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;
uniform sampler2D aTexture;

vec4 textureLookup(vec2 texCoord)
{
    return vec4(
        texture2D(yTexture, texCoord).r,
        texture2D(uTexture, texCoord).r - 0.5,
        texture2D(vTexture, texCoord).r - 0.5,
        texture2D(aTexture, texCoord).r);
}
