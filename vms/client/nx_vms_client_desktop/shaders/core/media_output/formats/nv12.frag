#version 330

uniform sampler2D yTexture;
uniform sampler2D uvTexture;

vec4 textureLookup(vec2 texCoord)
{
    return vec4(texture(yTexture, texCoord).r,
        texture(uvTexture, texCoord).rg - vec2(0.5),
        1.0);
}
