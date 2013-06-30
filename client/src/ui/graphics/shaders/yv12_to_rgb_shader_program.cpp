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
    uniform float xShift;

    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                1.0, -0.344, -0.714,  0.529,
                                1.0,  1.772,  0.0,   -0.886,
                                0.0,  0.0,    0.0,    opacity);

    const float PI = 3.1415926535;
    const float SRC_FOV = 180.0 * (PI / 180.0);
    const float DST_FOV = 90.0 * (PI / 180.0); // / SRC_FOV;
    const float ASPECT_RATIO = 4.0 / 3.0;
    const float PERSPECTIVE_ANGLE = 22.5 *  (PI / 180.0);

    void main() 
    {
        mat3 perspectiveMatrix = mat3( 1.0, 0.0,                    0.0,
                                       0.0, cos(PERSPECTIVE_ANGLE), -sin(PERSPECTIVE_ANGLE),
                                       0.0, sin(PERSPECTIVE_ANGLE), cos(PERSPECTIVE_ANGLE));


        vec2 pos = gl_TexCoord[0].st -0.5; // go to coordinates in range [-0.5..+0.5]

        float theta = xShift  + atan(pos.x * DST_FOV);
        float phi = atan(pos.y * DST_FOV) - PERSPECTIVE_ANGLE;

        // Vector in 3D space
        vec3 psph = vec3(cos(phi) * sin(theta),
                         cos(phi) * cos(theta),
                         sin(phi)             ) * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
        phi = atan(length(psph.xz),psph.y);
        float r = phi / SRC_FOV;

        vec2 srcCoord = vec2(0.5 + r * cos(theta), 0.5 + r * sin(theta)); // return from polar coordinates

        // do color transformation yuv->RGB
        gl_FragColor = vec4(texture2D(yTexture, srcCoord).p,
            texture2D(uTexture, srcCoord).p,
            texture2D(vTexture, srcCoord).p,
            1.0) * colorTransform;
    }
    ));

    link();

    m_yTextureLocation = uniformLocation("yTexture");
    m_uTextureLocation = uniformLocation("uTexture");
    m_vTextureLocation = uniformLocation("vTexture");
    m_opacityLocation = uniformLocation("opacity");
    m_xShiftLocation = uniformLocation("xShift");
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
    uniform float xShift;

    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0.0,  0.0,    0.0,    opacity);

    const float PI = 3.1415926535;
    const float SRC_FOV = 180.0 * (PI / 180.0);
    const float DST_FOV = 90.0 * (PI / 180.0);
    const float ASPECT_RATIO = 4.0 / 3.0;
    const float PERSPECTIVE_ANGLE = 22.5 *  (PI / 180.0);

    void main() 
    {
        mat3 perspectiveMatrix = mat3( 1.0, 0.0,                    0.0,
                                       0.0, cos(PERSPECTIVE_ANGLE), -sin(PERSPECTIVE_ANGLE),
                                       0.0, sin(PERSPECTIVE_ANGLE), cos(PERSPECTIVE_ANGLE));


        vec2 pos = gl_TexCoord[0].st -0.5; // go to coordinates in range [-0.5..+0.5]

        float theta = pos.x * DST_FOV + xShift;
        float phi   = pos.y * DST_FOV - PERSPECTIVE_ANGLE;

        // Vector in 3D space
        vec3 psph = vec3(cos(phi) * sin(theta),
                         cos(phi) * cos(theta),
                         sin(phi)             )  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
        phi = atan(length(psph.xz),psph.y);
        float r = phi / SRC_FOV;

        vec2 srcCoord = vec2(0.5 + r * cos(theta), 0.5 + r * sin(theta)); // return from polar coordinates

        // do color transformation yuv->RGB
        gl_FragColor = vec4(texture2D(yTexture, srcCoord).p,
            texture2D(uTexture, srcCoord).p,
            texture2D(vTexture, srcCoord).p,
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
    m_xShiftLocation = uniformLocation("xShift");
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
