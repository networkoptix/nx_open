#version 440

layout(location = 0) in vec2 projectionCoords;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 projectionCoordsScale;
    vec2 viewCenter;
    mat4 textureMatrix;
    mat4 viewRotationMatrix;
};

layout(binding = 1) uniform sampler2D sourceTexture;

const float pi = 3.1415926;

vec4 texture2DBlackBorder(vec2 coord)
{
    /* Turn outside areas to black without conditional operator: */
    bool isInside = all(bvec4(coord.x >= 0.0, coord.x <= 1.0, coord.y >= 0.0, coord.y <= 1.0));
    return texture(sourceTexture, coord) * vec4(vec3(float(isInside)), 1.0);
}

/* Projection functions should be written with the following consideration:
    *  a unit hemisphere is projected into a unit circle. */
vec2 project(vec3 coords);
vec3 unproject(vec2 coords);

void main()
{
    vec3 pointOnSphere = unproject(projectionCoords);
    vec3 rotatedPointOnSphere = (viewRotationMatrix * vec4(pointOnSphere, 1.0)).xyz;

    vec2 textureCoords = rotatedPointOnSphere.z < 0.999
        ? (textureMatrix * vec4(project(rotatedPointOnSphere), 0.0, 1.0)).xy
        : vec2(2.0); // somewhere outside

    fragColor = texture2DBlackBorder(textureCoords) * qt_Opacity;
}

vec2 project(vec3 coords)
{
    #if defined(lensProjectionType_Stereographic)
        return coords.xy / (1.0 - coords.z);
    #elif defined(lensProjectionType_Equisolid)
        return coords.xy / sqrt(1.0 - coords.z);
    #else // Equidistant
        float theta = acos(clamp(-coords.z, -1.0, 1.0));
        return coords.xy * (theta / (length(coords.xy) * (pi / 2.0)));
    #endif
}

vec3 unproject(vec2 coords)
{
    #if defined(viewProjectionType_Equidistant)
        float r = length(coords);
        float theta = clamp(r, 0.0, 2.0) * (pi / 2.0);
        return vec3(coords * sin(theta) / r, -cos(theta));
    #elif defined(viewProjectionType_Equisolid)
        float r2 = clamp(dot(coords, coords), 0.0, 2.0);
        return vec3(coords * sqrt(2.0 - r2), r2 - 1.0);
    #else // Stereographic
        float r2 = dot(coords, coords);
        return vec3(coords * 2.0, r2 - 1.0) / (r2 + 1.0);
    #endif
}
