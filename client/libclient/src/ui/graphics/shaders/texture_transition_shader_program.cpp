#include "texture_transition_shader_program.h"

#include "shader_source.h"

QnTextureTransitionShaderProgram::QnTextureTransitionShaderProgram(QObject *parent)
    : QnTextureGLShaderProgram(parent)
{
}

bool QnTextureTransitionShaderProgram::compile()
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
    uniform mat4 uModelViewProjectionMatrix;
    attribute vec2 aTexCoord;
    attribute vec4 aPosition;
    varying vec2 vTexColor;
    void main() {
        gl_Position = uModelViewProjectionMatrix * aPosition;
        vTexColor = aTexCoord;
    }
    ));

    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexColor;
    uniform vec4 uColor;
    uniform sampler2D uTexture;
    uniform sampler2D uTexture1;
    uniform float aProgress;
    void main() {
        vec4 texColor = texture2D(uTexture, vTexColor);
        vec4 texColor1 = texture2D(uTexture1, vTexColor);
        gl_FragColor = uColor * (texColor * (1.0 - aProgress) + texColor1 * aProgress);
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);

    return link();
}

