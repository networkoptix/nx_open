#include "color_shader_program.h"

QnColorShaderProgram::QnColorShaderProgram(const QGLContext *context, QObject *parent):
    QGLShaderProgram(context, parent)
{
    addShaderFromSourceCode(QGLShader::Vertex, "                                \
        void main() {                                                           \
            gl_FrontColor = gl_Color;                                           \
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;             \
        }                                                                       \
    ");
    addShaderFromSourceCode(QGLShader::Fragment, "                              \
        uniform vec4 colorMultiplier;                                           \
        void main() {                                                           \
            gl_FragColor = gl_Color * colorMultiplier;                          \
        }                                                                       \
    ");
    link();

    m_colorMultiplierLocation = uniformLocation("colorMultiplier");
}

