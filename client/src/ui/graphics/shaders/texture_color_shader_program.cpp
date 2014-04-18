#include "texture_color_shader_program.h"
#include "ui/graphics/shaders/shader_source.h"

QnTextureColorGLShaderProgramm::QnTextureColorGLShaderProgramm(const QGLContext *context, QObject *parent)
    : QnColorGLShaderProgramm(context,parent)
{
}
bool QnTextureColorGLShaderProgramm::compile()
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        attribute vec2 aTexCoord;
    attribute vec4 aPosition;
    uniform mat4 uModelViewProjectionMatrix;
    varying vec2 vTexColor;

    void main() {
        gl_Position = uModelViewProjectionMatrix * aPosition;
        vTexColor = aTexCoord;
    }
    ));
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        precision mediump float;
    varying vec2 vTexColor;
    uniform vec4 uColor;
    uniform sampler2D uTexture;
    void main() {
        vec4 texColor = texture2D(uTexture, vTexColor);
        gl_FragColor = uColor * texColor;
    }
    ));

    return link();
};
