#include "yv12_to_rgb_shader_program.h"

QnAbstractYv12ToRgbShaderProgram::QnAbstractYv12ToRgbShaderProgram(const QGLContext *context, QObject *parent):
    QGLShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        void main() {
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            gl_TexCoord[0] = gl_MultiTexCoord0;
    }
    ));

}


// ============================= QnYv12ToRgbShaderProgram ==================

QnYv12ToRgbShaderProgram::QnYv12ToRgbShaderProgram(const QGLContext *context, QObject *parent): 
    QnAbstractYv12ToRgbShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform float opacity;

        mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                    1.0, -0.344, -0.714,  0.529,
                                    1.0,  1.772,  0.0,   -0.886,
                                    0.0,  0.0,    0.0,    opacity);

        void main() {
            gl_FragColor = vec4(texture2D(yTexture, gl_TexCoord[0].st).p,
                                texture2D(uTexture, gl_TexCoord[0].st).p,
                                texture2D(vTexture, gl_TexCoord[0].st).p,
                                1.0) * colorTransform;
        }
    ));

    link();

    m_yTextureLocation = uniformLocation("yTexture");
    m_uTextureLocation = uniformLocation("uTexture");
    m_vTextureLocation = uniformLocation("vTexture");
    m_opacityLocation = uniformLocation("opacity");
}

    // ============================ QnYv12ToRgbWithGammaShaderProgram ==================================


QnYv12ToRgbWithGammaShaderProgram::QnYv12ToRgbWithGammaShaderProgram(const QGLContext *context, QObject *parent): 
    QnAbstractYv12ToRgbShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
    uniform sampler2D uTexture;
    uniform sampler2D vTexture;
    uniform float opacity;
    uniform float yLevels1;
    uniform float yLevels2;
    uniform float yGamma;

    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0.0,  0.0,    0.0,    opacity);

    void main() {
        float y = texture2D(yTexture, gl_TexCoord[0].st).p;
        gl_FragColor = vec4(clamp(pow(max(y+ yLevels2, 0.0) * yLevels1, yGamma), 0.0, 1.0),
                            texture2D(uTexture, gl_TexCoord[0].st).p,
                            texture2D(vTexture, gl_TexCoord[0].st).p,
                            1.0) * colorTransform;
    }
    ));

    link();

    m_yTextureLocation = uniformLocation("yTexture");
    m_uTextureLocation = uniformLocation("uTexture");
    m_vTextureLocation = uniformLocation("vTexture");
    m_opacityLocation = uniformLocation("opacity");
    m_yLevels1Location = uniformLocation("yLevels1");
    m_yLevels2Location = uniformLocation("yLevels2");
    m_yGammaLocation = uniformLocation("yGamma");
}


// ============================ QnYv12ToRgbaShaderProgram ==================================

QnYv12ToRgbaShaderProgram::QnYv12ToRgbaShaderProgram(const QGLContext *context, QObject *parent):
    QnAbstractYv12ToRgbShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform sampler2D aTexture;
        uniform float opacity;

        mat4 colorTransform = mat4( 1.0,  0.0,    1.402,  0.0,
                                    1.0, -0.344, -0.714,  0.0,
                                    1.0,  1.772,  0.0,   -0.0,
                                    0.0,  0.0,    0.0,    opacity);

        void main() {
            gl_FragColor = vec4(texture2D(yTexture, gl_TexCoord[0].st).p,
                                texture2D(uTexture, gl_TexCoord[0].st).p-0.5,
                                texture2D(vTexture, gl_TexCoord[0].st).p-0.5,
                                texture2D(aTexture, gl_TexCoord[0].st).p) * colorTransform;
        }
    ));

    link();

    m_yTextureLocation = uniformLocation("yTexture");
    m_uTextureLocation = uniformLocation("uTexture");
    m_vTextureLocation = uniformLocation("vTexture");
    m_aTextureLocation = uniformLocation("aTexture");
    m_opacityLocation = uniformLocation("opacity");
}
