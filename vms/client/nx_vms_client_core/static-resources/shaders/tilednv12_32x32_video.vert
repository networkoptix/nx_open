uniform highp mat4 qt_Matrix;
uniform highp float planeWidth;
uniform highp float planeHeight;
attribute highp vec4 qt_VertexPosition;
attribute highp vec2 qt_VertexTexCoord;
varying highp vec2 plane1TexCoord;

void main()
{
    plane1TexCoord = qt_VertexTexCoord * vec2(planeWidth, planeHeight) - vec2(0.5, 0.5);
    gl_Position = qt_Matrix * qt_VertexPosition;
}
