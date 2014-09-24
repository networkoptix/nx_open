#include "per_vertex_colored_shader_program.h"
#include "ui/graphics/shaders/shader_source.h"

QnColorPerVertexGLShaderProgram::QnColorPerVertexGLShaderProgram(QObject *parent)
    : QnColorGLShaderProgram(parent)
{
}
bool QnColorPerVertexGLShaderProgram::compile()
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
    uniform lowp mat4 uModelViewProjectionMatrix;
    attribute lowp vec4 aPosition;
    attribute lowp vec4 aColor;
    varying lowp vec4 vColor;

    void main() {
        gl_Position = uModelViewProjectionMatrix * aPosition;
        vColor = aColor;
    }
    ));

     QByteArray shader(QN_SHADER_SOURCE(
     uniform lowp vec4 uColor;
     varying lowp vec4 vColor;
     void main() {
         gl_FragColor = vColor*uColor;
     }
     ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);

    return link();
};
