#include "nv12_to_rgb_shader_program.h"

#include <utils/common/warnings.h>

#include "shader_source.h"

namespace {
    /** 
     * This is a matrix from Wikipedia, http://en.wikipedia.org/wiki/Yuv. 
     */
    float yuv_coef_ebu[4][4] = {
        { 1.0f,      1.0f,     1.0f,     0.0f },
        { 0.0f,     -0.3960f,  2.029f,   0.0f },
        { 1.140f,   -0.581f,   0.0f,     0.0f },
        { 0.0f,      0.0f,     0.0f,     1.0f }
    };

    float yuv_coef_bt601[4][4] = {
        { 1.0f,      1.0f,     1.0f,     0.0f },
        { 0.0f,     -0.344f,   1.773f,   0.0f },
        { 1.403f,   -0.714f,   0.0f,     0.0f },
        { 0.0f,      0.0f,     0.0f,     1.0f } 
    };

    float yuv_coef_bt709[4][4] = {
        { 1.0f,      1.0f,     1.0f,     0.0f },
        { 0.0f,     -0.1870f,  1.8556f,  0.0f },
        { 1.5701f,  -0.4664f,  0.0f,     0.0f },
        { 0.0f,      0.0f,     0.0f,     1.0f }
    };

    float yuv_coef_smtp240m[4][4] = {
        { 1.0f,      1.0f,     1.0f,     0.0f },
        { 0.0f,     -0.2253f,  1.8270f,  0.0f },
        { 1.5756f,  -0.5000f,  0.0f,     0.0f },
        { 0.0f,      0.0f,     0.0f,     1.0f }
    };

    float (&rawColorTransform(QnNv12ToRgbShaderProgram::Colorspace colorspace))[4][4] {
        switch(colorspace) {
        case QnNv12ToRgbShaderProgram::YuvEbu: return yuv_coef_ebu;
        case QnNv12ToRgbShaderProgram::YuvBt601: return yuv_coef_bt601;
        case QnNv12ToRgbShaderProgram::YuvBt709: return yuv_coef_bt709;
        case QnNv12ToRgbShaderProgram::YuvSmtp240m: return yuv_coef_smtp240m;
        default:
            qnWarning("Invalid colorspace '%1'.", static_cast<int>(colorspace));
            return yuv_coef_ebu; /* Sane default. */
        }
    }

} // anonymous namespace


QnNv12ToRgbShaderProgram::QnNv12ToRgbShaderProgram(QObject *parent):
    QnGLShaderProgram(parent),
    m_wasLinked(false)
{
    addShaderFromSourceCode(QOpenGLShader::Vertex, QN_SHADER_SOURCE(
        attribute vec4 aPosition;
        attribute vec2 aTexCoord;
        uniform mat4 uModelViewProjectionMatrix;
        varying vec2 vTexCoord;

        void main() {
            gl_Position = uModelViewProjectionMatrix * aPosition;
            vTexCoord = aTexCoord;
        }
    ));
    QByteArray shader(QN_SHADER_SOURCE(
    uniform sampler2D yTexture;
    uniform sampler2D uvTexture;
    uniform mat4 colorTransform;
    uniform float opacity;
    varying vec2 vTexCoord;

    void main() {
        vec4 yuv, rgb;
        yuv.rgba = vec4(
            texture(yTexture, vTexCoord.st).r,
            texture(uvTexture, vTexCoord.st).r,
            texture(uvTexture, vTexCoord.st).a,
            1.0
            );

        rgb = colorTransform * yuv;
        rgb.a = opacity;
        gl_FragColor = rgb;
    }
    ));
#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QOpenGLShader::Fragment, shader);
    m_wasLinked = link();

    m_yTextureLocation = uniformLocation("yTexture");
    m_uvTextureLocation = uniformLocation("uvTexture");
    m_colorTransformLocation = uniformLocation("colorTransform");
    m_opacityLocation = uniformLocation("opacity");
}

QMatrix4x4 QnNv12ToRgbShaderProgram::colorTransform(Colorspace colorspace, bool fullRange) {
    QMatrix4x4 result(reinterpret_cast<float *>(rawColorTransform(colorspace)));

    result.translate(0.0, -0.5, -0.5);

    if (!fullRange) {
        result.scale(255.0f / (235 - 16), 255.0f / (240 - 16), 255.0f / (240 - 16));
        result.translate(-16.0f / 255, -16.0f / 255, -16.0f / 255);
    }

    return result;
}

