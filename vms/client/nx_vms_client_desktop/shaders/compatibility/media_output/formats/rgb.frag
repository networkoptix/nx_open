#version 120

uniform sampler2D rgbTexture;

vec4 textureLookup(vec2 texCoord)
{
    return texture2D(rgbTexture, texCoord);
}
