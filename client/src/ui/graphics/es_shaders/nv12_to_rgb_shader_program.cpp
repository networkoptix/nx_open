#ifdef QT_OPENGL_ES_2
#include "nv12_to_rgb_shader_program.h"

#include <utils/common/warnings.h>

#include "shader_source.h"

namespace {
    /** 
     * This is a matrix from Wikipedia, http://en.wikipedia.org/wiki/Yuv. 
     */
    float yuv_coef_ebu[4][4] = {
        { 1.0,      1.0,     1.0,     0.0 },
        { 0.0,     -0.3960,  2.029,   0.0 },
        { 1.140,   -0.581,   0.0,     0.0 },
        { 0.0,      0.0,     0.0,     1.0 }
    };

    float yuv_coef_bt601[4][4] = {
        { 1.0,      1.0,     1.0,     0.0 },
        { 0.0,     -0.344,   1.773,   0.0 },
        { 1.403,   -0.714,   0.0,     0.0 },
        { 0.0,      0.0,     0.0,     1.0 } 
    };

    float yuv_coef_bt709[4][4] = {
        { 1.0,      1.0,     1.0,     0.0 },
        { 0.0,     -0.1870,  1.8556,  0.0 },
        { 1.5701,  -0.4664,  0.0,     0.0 },
        { 0.0,      0.0,     0.0,     1.0 }
    };

    float yuv_coef_smtp240m[4][4] = {
        { 1.0,      1.0,     1.0,     0.0 },
        { 0.0,     -0.2253,  1.8270,  0.0 },
        { 1.5756,  -0.5000,  0.0,     0.0 },
        { 0.0,      0.0,     0.0,     1.0 }
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


QnNv12ToRgbShaderProgram::QnNv12ToRgbShaderProgram(const QGLContext *context, QObject *parent):
    QnAbstractBaseGLShaderProgramm(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        attribute vec4 aPosition;
        attribute vec2 aTexCoord;
        uniform mat4 uModelViewProjectionMatrix;
        varying vec2 vTexCoord;

        void main() {
            gl_Position = uModelViewProjectionMatrix * aPosition;
            vTexCoord = aTexCoord;
        }
    ));
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        precision mediump float;
        uniform sampler2D yTexture;
        uniform sampler2D uvTexture;
        uniform mat4 colorTransform;
        uniform float opacity;
        varying vec2 vTexCoord;
                                                                                
        void main() {
            vec4 yuv, rgb;
            yuv.rgba = vec4(
                texture2D(yTexture, vTexCoord.st).r,
                texture2D(uvTexture, vTexCoord.st).r,
                texture2D(uvTexture, vTexCoord.st).a,
                1.0
            );
              
            rgb = colorTransform * yuv;
            rgb.a = opacity;
            gl_FragColor = rgb;
        }
    ));
    link();

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

#endif // QT_OPENGL_ES_2