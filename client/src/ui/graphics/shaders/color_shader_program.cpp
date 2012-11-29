#include "color_shader_program.h"

#include <ui/graphics/opengl/gl_context_data.h>

#include "shader_source.h"

Q_GLOBAL_STATIC(QnGlContextData<QnColorShaderProgram>, qn_colorShaderProgram_instanceStorage);

QnColorShaderProgram::QnColorShaderProgram(const QGLContext *context, QObject *parent):
    QGLShaderProgram(context, parent)
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        attribute vec4 color;
        varying vec4 fragColor;
        void main() {
            fragColor = color;
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        }
    ));
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        uniform vec4 colorMultiplier;
        varying vec4 fragColor;
        void main() {
            gl_FragColor = fragColor * colorMultiplier;
        }
    ));
    link();

    m_colorMultiplierLocation = uniformLocation("colorMultiplier");
    m_colorLocation = attributeLocation("color");
}

QSharedPointer<QnColorShaderProgram> QnColorShaderProgram::instance(const QGLContext *context) {
    return qn_colorShaderProgram_instanceStorage()->get(context);
}