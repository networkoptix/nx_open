#include "yv12_to_rgb_shader_program.h"
#include <cmath> /* For std::sin & std::cos. */

QnYv12ToRgbShaderProgram::QnYv12ToRgbShaderProgram(const QGLContext *context, QObject *parent): 
    QGLShaderProgram(context, parent) 
{
    m_isValid = addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        void main() {
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            gl_TexCoord[0] = gl_MultiTexCoord0;
        }
    ));

    m_isValid &= addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform float opacity;

        mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                    1.0, -0.344, -0.714,  0.529,
                                    1.0,  1.772,  0.0,   -0.886,
                                    0,    0,      0,      opacity);

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


QnYv12ToRgbaShaderProgram::QnYv12ToRgbaShaderProgram(const QGLContext *context, QObject *parent):
    QnArbShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        !!ARBfp1.0
        PARAM c[5] =    { program.local[0..1],
                        { 1.164, 0, 1.596, 0.5 },
                        { 0.0625, 1.164, -0.391, -0.81300002 },
                        { 1.164, 2.0179999, 0 } };
        TEMP R0;
        TEX R0.x, fragment.texcoord[0], texture[1], 2D;
        ADD R0.y, R0.x, -c[2].w;
        TEX R0.x, fragment.texcoord[0], texture[2], 2D;
        ADD R0.x, R0, -c[2].w;
        MUL R0.z, R0.y, c[0].w;
        MAD R0.z, R0.x, c[0], R0;
        MUL R0.w, R0.x, c[0];
        MUL R0.z, R0, c[0].y;
        TEX R0.x, fragment.texcoord[0], texture[0], 2D;
        MAD R0.y, R0, c[0].z, R0.w;
        ADD R0.x, R0, -c[3];
        MUL R0.y, R0, c[0];
        MUL R0.z, R0, c[1].x;
        MAD R0.x, R0, c[0].y, c[0];
        MUL R0.y, R0, c[1].x;
        DP3 result.color.x, R0, c[2];
        DP3 result.color.y, R0, c[3].yzww;
        DP3 result.color.z, R0, c[4];
        TEX R0.x, fragment.texcoord[0], texture[3], 2D;
        MUL R0.x, R0, c[1].y;
        MOV result.color.w, R0.x;
        END
    ));
}

void QnYv12ToRgbaShaderProgram::setParameters(GLfloat brightness, GLfloat contrast, GLfloat hue, GLfloat saturation, GLfloat opacity) {
    setLocalValue(QGLShader::Fragment, 0, brightness, contrast, std::cos(hue), std::sin(hue));
    setLocalValue(QGLShader::Fragment, 1, saturation, opacity, 0.0, 0.0);
}
