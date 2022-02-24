#version 120

varying highp vec2 fragTexCoord;

uniform mediump mat4 colorMatrix;

vec2 textureTransform(vec2 texCoord);
vec4 textureLookup(vec2 texCoord);
vec4 colorCorrection(vec4 source);

void main()
{
    gl_FragColor = colorMatrix * colorCorrection(textureLookup(textureTransform(fragTexCoord)));
}
