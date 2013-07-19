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

bool QnAbstractYv12ToRgbShaderProgram::link()
{
    bool rez = QGLShaderProgram::link();
    if (rez) {
        m_yTextureLocation = uniformLocation("yTexture");
        m_uTextureLocation = uniformLocation("uTexture");
        m_vTextureLocation = uniformLocation("vTexture");
        m_opacityLocation = uniformLocation("opacity");
    }
    return rez;
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

    void main() 
    {
        vec2 pos = gl_TexCoord[0].st;
        // do color transformation yuv->RGB
        gl_FragColor = vec4(texture2D(yTexture, pos).p,
                            texture2D(uTexture, pos).p,
                            texture2D(vTexture, pos).p,
                            1.0) * colorTransform;
    }
    ));

    link();
}

// ============================ QnYv12ToRgbWithGammaShaderProgram ==================================

bool QnYv12ToRgbWithGammaShaderProgram::link()
{
    bool rez = QnAbstractYv12ToRgbShaderProgram::link();
    if (rez) {
        m_yLevels1Location = uniformLocation("yLevels1");
        m_yLevels2Location = uniformLocation("yLevels2");
        m_yGammaLocation = uniformLocation("yGamma");
    }
    return rez;
}

QnYv12ToRgbWithGammaShaderProgram::QnYv12ToRgbWithGammaShaderProgram(const QGLContext *context, QObject *parent, bool final): 
    QnAbstractYv12ToRgbShaderProgram(context, parent) 
{
    if (!final)
        return;
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
}


// ============================= QnYv12ToRgbWithFisheyeShaderProgram ==================

bool QnFisheyeShaderProgram::link()
{
    bool rez = QnYv12ToRgbWithGammaShaderProgram::link();
    if (rez) {
        m_xShiftLocation = uniformLocation("xShift");
        m_yShiftLocation = uniformLocation("yShift");
        m_fovRotLocation = uniformLocation("fovRot");
        m_dstFovLocation = uniformLocation("dstFov");
        m_aspectRatioLocation = uniformLocation("aspectRatio");
        m_yCenterLocation = uniformLocation("yCenter");

        m_maxXLocation = uniformLocation("maxX");
        m_maxYLocation = uniformLocation("maxY");
    }
    return rez;
}

QnFisheyeShaderProgram::QnFisheyeShaderProgram(const QGLContext *context, QObject *parent, const QString& gammaStr):
    QnYv12ToRgbWithGammaShaderProgram(context, parent, false) 
{
    addShaderFromSourceCode(QGLShader::Fragment, getShaderText().arg(gammaStr));

    link();
}

// ---------------------------- QnFisheyeHorizontalShaderProgram ------------------------------------

QString QnFisheyeShaderProgram::getShaderText()
{
    return lit(QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform float opacity;
        uniform float xShift;
        uniform float yShift;
        uniform float fovRot;
        uniform float dstFov;
        uniform float aspectRatio;
        uniform float yCenter;
        uniform float yLevels1;
        uniform float yLevels2;
        uniform float yGamma;
        uniform float maxX;
        uniform float maxY;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                1.0, -0.344, -0.714,  0.529,
                                1.0,  1.772,  0.0,   -0.886,
                                0.0,  0.0,    0.0,    opacity);

    vec3 sphericalToCartesian(in float theta, in float phi) {
        return vec3(cos(phi) * sin(theta), cos(phi)*cos(theta), sin(phi));
    }

    float kx =  2.0*tan(dstFov/2.0);
    mat3 to3d = transpose(mat3(
        sphericalToCartesian(xShift + PI/2.0, 0.0) * kx,
        sphericalToCartesian(xShift, -yShift + PI/2.0) * kx /aspectRatio,
        sphericalToCartesian(xShift, -yShift)));

    void main() 
    {
        vec3 pos3d = vec3(gl_TexCoord[0].xy - vec2(0.5, yCenter), 1.0) * to3d; // point on the surface
        float theta = atan(pos3d.z, pos3d.x) + fovRot;                         // fisheye angle
        float r     = acos(pos3d.y / length(pos3d)) / PI;                      // fisheye radius
        vec2 pos = vec2(cos(theta), sin(theta)) * r + 0.5;                     // return from polar

        pos.x = min(pos.x, maxX);
        pos.y = min(pos.y, maxY);

        // do gamma correction and color transformation yuv->RGB
        float y = texture2D(yTexture, pos).p;
        gl_FragColor = vec4(%1, // put gamma correction str here or just 'y'
                            texture2D(uTexture, pos).p,
                            texture2D(vTexture, pos).p,
                            1.0) * colorTransform;
    }
    ));
}

// ============================ QnYv12ToRgbaShaderProgram ==================================

bool QnYv12ToRgbaShaderProgram::link()
{
    bool rez = QnAbstractYv12ToRgbShaderProgram::link();
    if (rez)
        m_aTextureLocation = uniformLocation("aTexture");
    return rez;
}

QnYv12ToRgbaShaderProgram::QnYv12ToRgbaShaderProgram(const QGLContext *context, QObject *parent):
    QnAbstractYv12ToRgbShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Fragment, QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform float opacity;
        uniform sampler2D aTexture;

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
}
