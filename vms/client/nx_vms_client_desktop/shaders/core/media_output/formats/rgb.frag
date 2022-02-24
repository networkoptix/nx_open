#version 330

uniform sampler2D rgbTexture;

vec4 textureLookup(vec2 texCoord)
{
    return texture(rgbTexture, texCoord);
}
