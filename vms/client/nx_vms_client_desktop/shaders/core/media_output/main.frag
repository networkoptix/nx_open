#version 330

in highp vec2 fragTexCoord;
layout(location = 0, index = 0) out vec4 fragColor;

uniform mediump mat4 colorMatrix;

vec2 textureTransform(vec2 texCoord);
vec4 textureLookup(vec2 texCoord);
vec4 colorCorrection(vec4 source);

void main()
{
    fragColor = colorMatrix * colorCorrection(textureLookup(textureTransform(fragTexCoord)));
}
