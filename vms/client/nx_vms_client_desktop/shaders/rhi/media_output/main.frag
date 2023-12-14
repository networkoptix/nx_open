#version 440

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf
{
    mat4 uModelViewProjectionMatrix; // offset 0
    mat3 uTexCoordMatrix; // offset 64
    mat4 colorMatrix; // offset 112
    #if defined(yuv_gamma)
        float gamma; // offset 176
        float luminocityShift; // offset 180
        float luminocityFactor; // offset 184
    #endif
    #if defined(dewarping)
        mat3 texCoordFragMatrix; // offset 176 or 192 (with yuv_gamma)

        // sphereRotation for equirectangular, unprojectionMatrix for rectilinear
        mat3 sphereRotationOrUnprojectionMatrix; // offset 224 or 240 (with yuv_gamma)

        #if defined(camera_equirectangular360)
            mat3 rotationCorrectionMatrix;  // offset 272 or 288 (with yuv_gamma)
        #endif
    #endif
};

#if defined(dewarping)
vec2 cameraProject(vec3 pointOnSphere)
{
    #if defined(camera_equidistant)
        const float kHalfPi = 3.1415926535 / 2.0;
        float theta = acos(clamp(pointOnSphere.y, -1.0, 1.0));
        return pointOnSphere.xz * (theta / (length(pointOnSphere.xz) * kHalfPi));
    #elif defined(camera_stereographic)
        return pointOnSphere.xz / (1.0 + pointOnSphere.y);
    #elif defined(camera_equisolid)
        return pointOnSphere.xz / sqrt(1.0 + pointOnSphere.y);
    #elif defined(camera_equirectangular360)
        vec3 point = rotationCorrectionMatrix * pointOnSphere;
        return vec2(atan(point.x, point.y), asin(point.z));
    #endif
}

vec3 viewUnproject(vec2 viewCoord)
{
    #if defined(view_rectilinear)
        return sphereRotationOrUnprojectionMatrix * vec3(viewCoord, 1.0);
    #elif defined(view_equirectangular)
        float cosTheta = cos(viewCoord.x);
        float sinTheta = sin(viewCoord.x);

        float cosPhi = cos(viewCoord.y);
        float sinPhi = sin(viewCoord.y);

        return vec3(cosPhi * sinTheta, cosPhi * cosTheta, sinPhi)
            * sphereRotationOrUnprojectionMatrix;
    #endif
}

vec2 textureTransform(vec2 texCoord)
{
    // TODO: #vkutin Get rid of "normalize", unproject functions should yield unit vectors.
    return (texCoordFragMatrix * vec3(cameraProject(normalize(viewUnproject(texCoord))), 1.0)).xy;
}
#else
vec2 textureTransform(vec2 texCoord)
{
    return texCoord;
}
#endif

#if defined(format_nv12)
layout(binding = 1) uniform sampler2D yTexture;
layout(binding = 2) uniform sampler2D uvTexture;

vec4 textureLookup(vec2 texCoord)
{
    return vec4(texture(yTexture, texCoord).r,
        texture(uvTexture, texCoord).rg - vec2(0.5),
        1.0);
}
#elif defined(format_rgb)
layout(binding = 1) uniform sampler2D rgbTexture;

vec4 textureLookup(vec2 texCoord)
{
    return texture(rgbTexture, texCoord);
}
#elif defined(format_yv12)
layout(binding = 1) uniform sampler2D yTexture;
layout(binding = 2) uniform sampler2D uTexture;
layout(binding = 3) uniform sampler2D vTexture;

vec4 textureLookup(vec2 texCoord)
{
    return vec4(
        texture(yTexture, texCoord).r,
        texture(uTexture, texCoord).r - 0.5,
        texture(vTexture, texCoord).r - 0.5,
        1.0);
}
#elif defined(format_yva12)
layout(binding = 1) uniform sampler2D yTexture;
layout(binding = 2) uniform sampler2D uTexture;
layout(binding = 3) uniform sampler2D vTexture;
layout(binding = 4) uniform sampler2D aTexture;

vec4 textureLookup(vec2 texCoord)
{
    return vec4(
        texture(yTexture, texCoord).r,
        texture(uTexture, texCoord).r - 0.5,
        texture(vTexture, texCoord).r - 0.5,
        texture(aTexture, texCoord).r);
}
#endif

#if defined(yuv_gamma)
vec4 colorCorrection(vec4 source)
{
    return vec4(
        clamp(pow(max(source.r + luminocityShift, 0.0) * luminocityFactor, gamma), 0.0, 1.0),
        source.gba);
}
#else
vec4 colorCorrection(vec4 source)
{
    return source;
}
#endif


void main()
{
    fragColor = colorMatrix * colorCorrection(textureLookup(textureTransform(fragTexCoord)));
}
