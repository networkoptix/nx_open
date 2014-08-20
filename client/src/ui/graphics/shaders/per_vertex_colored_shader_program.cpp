#include "per_vertex_colored_shader_program.h"
#include "ui/graphics/shaders/shader_source.h"

QnColorPerVertexGLShaderProgram::QnColorPerVertexGLShaderProgram(const QGLContext *context, QObject *parent)
    : QnColorGLShaderProgram(context,parent)
{
}
bool QnColorPerVertexGLShaderProgram::compile()
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        uniform mat4 uModelViewProjectionMatrix;
    attribute vec4 aPosition;
    attribute vec4 aColor;
    varying vec4 vColor;
    void main() {
        gl_Position = uModelViewProjectionMatrix * aPosition;
        vColor = aColor;
    }
    ));

     QByteArray shader(QN_SHADER_SOURCE(
     uniform vec4 uColor;
     varying vec4 vColor;
     void main() {
         gl_FragColor = uColor*vColor;
     }
     ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QGLShader::Fragment, shader);

    return link();
};
