#include "color_shader_program.h"

#include <ui/graphics/opengl/gl_context_data.h>
#include "ui/graphics/shaders/shader_source.h"
#include "shader_source.h"


QnColorGLShaderProgram::QnColorGLShaderProgram(const QGLContext *context, QObject *parent)
    : QnGLShaderProgram(context,parent)
{
}

bool QnColorGLShaderProgram::compile()
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        uniform mat4 uModelViewProjectionMatrix;
        attribute vec4 aPosition;
        void main() {
            gl_Position = uModelViewProjectionMatrix * aPosition;
        }
    ));

    QByteArray shader(QN_SHADER_SOURCE(
        uniform vec4 uColor;
        void main() {
            gl_FragColor = uColor;
        }
    ));

#ifdef QT_OPENGL_ES_2
    shader = QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QGLShader::Fragment, shader);

    return link();
};
