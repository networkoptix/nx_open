#include "per_vertex_colored_shader_program.h"
#include "ui/graphics/shaders/shader_source.h"

QnPerVertexColoredGLShaderProgramm::QnPerVertexColoredGLShaderProgramm(const QGLContext *context, QObject *parent)
    : QnColorGLShaderProgramm(context,parent)
{
}
bool QnPerVertexColoredGLShaderProgramm::compile()
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
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        precision mediump float;
    uniform vec4 uColor;
    varying vec4 vColor;
    void main() {
        gl_FragColor = uColor*vColor;
    }
    ));

    return link();
};
