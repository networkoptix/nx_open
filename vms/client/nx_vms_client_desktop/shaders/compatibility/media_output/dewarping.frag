#version 120

uniform highp mat3 texCoordFragMatrix;

vec2 cameraProject(vec3 pointOnSphere);
vec3 viewUnproject(vec2 viewCoord);

vec2 textureTransform(vec2 texCoord)
{
    // TODO: #vkutin Get rid of "normalize", unproject functions should yield unit vectors.
    return (texCoordFragMatrix * vec3(cameraProject(normalize(viewUnproject(texCoord))), 1.0)).xy;
}
