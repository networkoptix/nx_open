#include "texture_color_shader_program.h"
#include "ui/graphics/shaders/shader_source.h"

QnTextureGLShaderProgram::QnTextureGLShaderProgram(QObject *parent)
    : QnColorGLShaderProgram(parent)
{
}
bool QnTextureGLShaderProgram::compile()
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
        attribute vec2 aTexCoord;
    attribute vec4 aPosition;
    uniform mat4 uModelViewProjectionMatrix;
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
    void main() {
        vec4 texColor = texture2D(uTexture, vTexColor);
        gl_FragColor = uColor * texColor;
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);


    return link();
};
