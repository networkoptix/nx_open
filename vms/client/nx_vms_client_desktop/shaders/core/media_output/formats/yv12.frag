#version 330

uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;

vec4 textureLookup(vec2 texCoord)
{
    return vec4(
        texture(yTexture, texCoord).r,
        texture(uTexture, texCoord).r - 0.5,
        texture(vTexture, texCoord).r - 0.5,
        1.0);
}
