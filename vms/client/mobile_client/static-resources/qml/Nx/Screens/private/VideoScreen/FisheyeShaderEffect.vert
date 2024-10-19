#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(location = 0) out vec2 projectionCoords; // carthesian coords local to hemispere projection unit circle

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 projectionCoordsScale;
    vec2 viewCenter;
    mat4 textureMatrix;
    mat4 viewRotationMatrix;
};

void main()
{
    projectionCoords = (qt_MultiTexCoord0 - viewCenter) * projectionCoordsScale;
    gl_Position = qt_Matrix * qt_Vertex;
}
