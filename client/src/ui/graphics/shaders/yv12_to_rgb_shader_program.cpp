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
        m_perspShiftLocation = uniformLocation("perspShift");
        m_dstFovLocation = uniformLocation("dstFov");
        m_aspectRatioLocation = uniformLocation("aspectRatio");
    }
    return rez;
}

QnFisheyeShaderProgram::QnFisheyeShaderProgram(const QGLContext *context, QObject *parent, const QString& shaderStr):
    QnYv12ToRgbWithGammaShaderProgram(context, parent, false) 
{
    addShaderFromSourceCode(QGLShader::Fragment, shaderStr);

    link();
}

// ---------------------------- QnFisheyeHorizontalShaderProgram ------------------------------------

QnFisheyeHorizontalShaderProgram::QnFisheyeHorizontalShaderProgram(const QGLContext *context, QObject *parent, const QString& gammaStr):
    QnFisheyeShaderProgram(context, parent, getShaderText().arg(gammaStr))
{

}

QString QnFisheyeHorizontalShaderProgram::getShaderText()
{
    return lit(QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform float opacity;
        uniform float xShift;
        uniform float yShift;
        uniform float perspShift;
        uniform float dstFov;
        uniform float aspectRatio;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                1.0, -0.344, -0.714,  0.529,
                                1.0,  1.772,  0.0,   -0.886,
                                0.0,  0.0,    0.0,    opacity);

    vec3 sphericalToCartesian(float theta, float phi) {
        return vec3(cos(phi) * sin(theta), cos(phi)*cos(theta), sin(phi));
    }

    vec3 center = sphericalToCartesian(xShift, -yShift);
    vec3 x  = sphericalToCartesian(xShift + PI/2.0, 0.0) * 2.0*tan(dstFov/2.0);
    vec3 y  = sphericalToCartesian(xShift, -yShift + PI/2.0) * 2.0*tan(dstFov/2.0);

    mat3 to3d = mat3(x.x, y.x, center.x,
        x.y, y.y, center.y,
        x.z, y.z, center.z);

    float pShift = sin(xShift)*sin(perspShift);

    void main() 
    {

        vec2 pos = gl_TexCoord[0].st - 0.5; // go to coordinates in range [-0.5..0.5]
        pos.y = pos.y / aspectRatio;

        vec3 pos3d = vec3(pos.x, pos.y, 1.0) * to3d;

        float theta = atan(pos3d.x, pos3d.y);
        float phi   = asin(pos3d.z / length(pos3d));

        // Vector in 3D space
        vec3 psph = sphericalToCartesian(theta, phi); // * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x) + pShift;
        phi   = atan(length(psph.xz), psph.y);
        float r = phi / PI; // fisheye FOV

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r + 0.5;

        // do color transformation yuv->RGB
        gl_FragColor = vec4(texture2D(yTexture, pos).p,
            texture2D(uTexture, pos).p,
            texture2D(vTexture, pos).p,
            1.0) * colorTransform;
    }
    ));
}

// ------------------ QnFisheyeVerticalShaderProgram -------------------

QnFisheyeVerticalShaderProgram::QnFisheyeVerticalShaderProgram(const QGLContext *context, QObject *parent, const QString& gammaStr):
    QnFisheyeShaderProgram(context, parent, getShaderText().arg(gammaStr))
{

}


QString QnFisheyeVerticalShaderProgram::getShaderText() 
{
    return lit(QN_SHADER_SOURCE(
        uniform sampler2D yTexture;
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        uniform float opacity;
        uniform float xShift;
        uniform float yShift;
        uniform float perspShift;
        uniform float dstFov;
        uniform float aspectRatio;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                1.0, -0.344, -0.714,  0.529,
                                1.0,  1.772,  0.0,   -0.886,
                                0.0,  0.0,    0.0,    opacity);

    vec3 sphericalToCartesian(float theta, float phi) {
        return vec3(cos(phi) * sin(theta), cos(phi)*cos(theta), sin(phi));
    }

    float xShift2 = sin(xShift)*sin(perspShift);
    vec3 center = sphericalToCartesian(xShift2, -yShift);
    vec3 x  = sphericalToCartesian(xShift2 + PI/2.0, 0.0) * 2.0*tan(dstFov/2.0);
    vec3 y  = sphericalToCartesian(xShift2, -yShift + PI/2.0) * 2.0*tan(dstFov/2.0);

    mat3 to3d = mat3(x.x, y.x, center.x,
                     x.y, y.y, center.y,
                     x.z, y.z, center.z);

    void main() 
    {

        vec2 pos = gl_TexCoord[0].st - 0.5; // go to coordinates in range [-0.5..0.5]
        pos.y = pos.y - 0.5;
        pos.y = pos.y / aspectRatio;

        vec3 pos3d = vec3(pos.x, pos.y, 1.0) * to3d;

        float theta = atan(pos3d.x, pos3d.y);
        float phi   = asin(pos3d.z / length(pos3d));

        // Vector in 3D space
        vec3 psph = sphericalToCartesian(theta, phi); // * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x) - xShift;
        phi   = atan(length(psph.xz), psph.y);
        float r = phi / PI; // fisheye FOV

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r + 0.5;

        // do color transformation yuv->RGB
        gl_FragColor = vec4(texture2D(yTexture, pos).p,
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
