#include "texture_transition_shader_program.h"

#include "shader_source.h"

QnTextureTransitionShaderProgram::QnTextureTransitionShaderProgram(const QGLContext *context, QObject *parent):
    QGLShaderProgram(context, parent)
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        void main() {
            gl_FrontColor = gl_Color;
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            gl_TexCoord[0] = gl_MultiTexCoord0;
        }
    ));
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        uniform sampler2D texture0;
        uniform sampler2D texture1;
        uniform float progress;
        
        void main() {
            vec4 color0 = texture2D(texture0, gl_TexCoord[0].st);
            vec4 color1 = texture2D(texture1, gl_TexCoord[0].st);
            gl_FragColor = gl_Color * (color0 * progress + color1 * (1.0 - progress));
        }
    ));
    link();

    m_texture0Location = uniformLocation("texture0");
    m_texture1Location = uniformLocation("texture1");
    m_progressLocation = uniformLocation("progress");
}

