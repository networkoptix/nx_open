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

const QString QnFisheyeShaderProgram::GAMMA_STRING(lit("clamp(pow(max(y+ yLevels2, 0.0) * yLevels1, yGamma), 0.0, 1.0"));

QnFisheyeShaderProgram::QnFisheyeShaderProgram(const QGLContext *context, QObject *parent, const QString& gammaStr):
    QnYv12ToRgbWithGammaShaderProgram(context, parent, false),
    m_gammaStr(gammaStr)
{
}


void QnFisheyeShaderProgram::setDewarpingParams(const DewarpingParams& params, float aspectRatio, float maxX, float maxY)
{
    if (params.panoFactor == 1.0)
    {
        float fovRot = sin(params.xAngle)*params.fovRot;
        if (params.horizontalView) {
            setUniformValue(m_yShiftLocation, (float) (params.yAngle));
            setUniformValue(m_yCenterLocation, (float) 0.5);
            setUniformValue(m_xShiftLocation, (float) params.xAngle);
            setUniformValue(m_fovRotLocation, (float) fovRot);
        }
        else {
            setUniformValue(m_yShiftLocation, (float) (params.yAngle - M_PI/2.0));
            setUniformValue(m_yCenterLocation, (float) 1.0);
            setUniformValue(m_xShiftLocation, (float) fovRot);
            setUniformValue(m_fovRotLocation, (float) -params.xAngle);
        }
    }
    else {
        setUniformValue(m_xShiftLocation, (float) params.xAngle);
        setUniformValue(m_fovRotLocation, (float) params.fovRot);
        setUniformValue(m_yShiftLocation, (float) (params.yAngle));
        if (params.horizontalView)
            setUniformValue(m_yCenterLocation, (float) 0.5);
        else
            setUniformValue(m_yCenterLocation, (float) 1.0);
    }

    setUniformValue(m_aspectRatioLocation, (float) (aspectRatio * params.panoFactor));
    setUniformValue(m_dstFovLocation, (float) params.fov);

    setUniformValue(m_maxXLocation, maxX);
    setUniformValue(m_maxYLocation, maxY);
}

bool QnFisheyeShaderProgram::link()
{
    addShaderFromSourceCode(QGLShader::Fragment, getShaderText().arg(m_gammaStr));
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

// ---------------------------- QnFisheyeRectilinearProgram ------------------------------------

QnFisheyeRectilinearProgram::QnFisheyeRectilinearProgram(const QGLContext *context, QObject *parent, const QString& gammaStr):
    QnFisheyeShaderProgram(context, parent, gammaStr)
{

}

QString QnFisheyeRectilinearProgram::getShaderText()
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

    float kx =  2.0*tan(dstFov/2.0);

    //vec3 sphericalToCartesian(in float theta, in float phi) {
    //    return vec3(cos(phi) * sin(theta), cos(phi)*cos(theta), sin(phi));
    //}
    //vec3 xVect  = vec3(sphericalToCartesian(xShift + PI/2.0, 0.0)) * kx;
    //vec3 yVect  = vec3(sphericalToCartesian(xShift, -yShift + PI/2.0)) * kx /aspectRatio;
    //vec3 center = vec3(sphericalToCartesian(xShift, -yShift));

    // avoid function call for better shader compatibility
    vec3 xVect  = vec3(sin(xShift + PI/2.0), cos(xShift + PI/2.0), 0.0) * kx; 
    vec3 yVect  = vec3(cos(-yShift + PI/2.0) * sin(xShift), cos(-yShift + PI/2.0)*cos(xShift), sin(-yShift + PI/2.0)) * kx /aspectRatio;
    vec3 center = vec3(cos(-yShift) * sin(xShift), cos(-yShift)*cos(xShift), sin(-yShift));

    mat3 to3d = mat3(xVect.x,   yVect.x,    center.x,    
                     xVect.y,   yVect.y,    center.y,
                     xVect.z,   yVect.z,    center.z); // avoid transpose it is required glsl 1.2
    
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
        gl_FragColor = vec4(%1,
                            texture2D(uTexture, pos).p,
                            texture2D(vTexture, pos).p,
                            1.0) * colorTransform;
    }
    ));
}

// ------------------------- QnFisheyeEquirectangularHProgram -----------------------------

QnFisheyeEquirectangularHProgram::QnFisheyeEquirectangularHProgram(const QGLContext *context, QObject *parent, const QString& gammaStr)
    :QnFisheyeShaderProgram(context, parent, gammaStr)
{

}

QString QnFisheyeEquirectangularHProgram::getShaderText()
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
    
    mat3 perspectiveMatrix = mat3( 1.0, 0.0,              0.0,
                                   0.0, cos(-fovRot), -sin(-fovRot),
                                   0.0, sin(-fovRot),  cos(-fovRot));

    void main() 
    {
        vec2 pos = (gl_TexCoord[0].xy - vec2(0.5, yCenter)) * dstFov; // go to coordinates in range [-dstFov/2..+dstFov/2]

        float theta = pos.x + xShift;
        float roty = -fovRot* cos(theta);
        pos.y = pos.y/(-aspectRatio);
        float ymaxInv = aspectRatio / dstFov;
        float phi   = pos.y * ((ymaxInv - roty) / ymaxInv) + roty + yShift;

        // Vector in 3D space
        vec3 psph = vec3(cos(phi) * sin(theta),
                        cos(phi) * cos(theta),
                        sin(phi)             )  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
        phi   = acos(psph.y);
        float r = phi / PI; // fisheye FOV

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r + 0.5;

        pos.x = min(pos.x, maxX);
        pos.y = min(pos.y, maxY);

        // do gamma correction and color transformation yuv->RGB
        float y = texture2D(yTexture, pos).p;
        gl_FragColor = vec4(%1,
                            texture2D(uTexture, pos).p,
                            texture2D(vTexture, pos).p,
                            1.0) * colorTransform;
    }
    ));
}

// ----------------------------------------- QnFisheyeEquirectangularVProgram ---------------------------------------

QnFisheyeEquirectangularVProgram::QnFisheyeEquirectangularVProgram(const QGLContext *context, QObject *parent, const QString& gammaStr)
    :QnFisheyeShaderProgram(context, parent, gammaStr)
{

}


QString QnFisheyeEquirectangularVProgram::getShaderText()
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

    mat3 perspectiveMatrix = mat3( 1.0, 0.0,              0.0,
                                   0.0, cos(-fovRot), -sin(-fovRot),
                                   0.0, sin(-fovRot),  cos(-fovRot));

    void main() 
    {
        vec2 pos = (gl_TexCoord[0].xy - vec2(0.5, yCenter)) * dstFov; // go to coordinates in range [-dstFov/2..+dstFov/2]

        float theta = pos.x + xShift;
        float roty = -fovRot* cos(theta);
        float roty2 = -fovRot*2.0*cos(theta);
        
        pos.y = pos.y/(-aspectRatio);
        float ymaxInv = aspectRatio / dstFov;
        float phi   = pos.y * ((ymaxInv - roty) / ymaxInv) + roty + yShift;
        
        // Vector in 3D space
        vec3 psph = vec3(cos(phi) * sin(theta),
                         sin(phi),  // only one difference between H and V shaders: this 2 lines in back order
                         cos(phi) * cos(theta))  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
        phi   = acos(psph.y);
        float r = phi / PI; // fisheye FOV

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r + 0.5;

        pos.x = min(pos.x, maxX);
        pos.y = min(pos.y, maxY);

        // do gamma correction and color transformation yuv->RGB
        float y = texture2D(yTexture, pos).p;
        gl_FragColor = vec4(%1,
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
