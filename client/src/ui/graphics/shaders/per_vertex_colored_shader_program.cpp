#include "per_vertex_colored_shader_program.h"
#include "ui/graphics/shaders/shader_source.h"

QnColorPerVertexGLShaderProgram::QnColorPerVertexGLShaderProgram(const QGLContext *context, QObject *parent)
    : QnColorGLShaderProgram(context,parent)
{
}
bool QnColorPerVertexGLShaderProgram::compile()
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
    uniform lowp mat4 uModelViewProjectionMatrix;
    uniform lowp vec4 uColor;
    attribute lowp vec4 aPosition;
    attribute lowp vec4 aColor;
    varying lowp vec4 vColor;

    void main() {
        gl_Position = uModelViewProjectionMatrix * aPosition;
        vColor = aColor*uColor;
    }
    ));

     QByteArray shader(QN_SHADER_SOURCE(
     varying vec4 vColor;
     void main() {
         gl_FragColor = vColor;
     }
     ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QGLShader::Fragment, shader);

    return link();
};
