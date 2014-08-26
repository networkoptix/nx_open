#include "yv12_to_rgb_shader_program.h"

QnAbstractYv12ToRgbShaderProgram::QnAbstractYv12ToRgbShaderProgram(const QGLContext *context, QObject *parent):
    QnGLShaderProgram(context, parent),
    m_wasLinked(false)
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

}

bool QnAbstractYv12ToRgbShaderProgram::link()
{
    m_wasLinked = QnGLShaderProgram::link();
    if (m_wasLinked) {
        m_yTextureLocation = uniformLocation("yTexture");
        m_uTextureLocation = uniformLocation("uTexture");
        m_vTextureLocation = uniformLocation("vTexture");
        m_opacityLocation = uniformLocation("opacity");
    }
    return m_wasLinked;
}

// ============================= QnYv12ToRgbShaderProgram ==================

QnYv12ToRgbShaderProgram::QnYv12ToRgbShaderProgram(const QGLContext *context, QObject *parent): 
    QnAbstractYv12ToRgbShaderProgram(context, parent) 
{

    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexCoord;

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
        // do color transformation yuv->RGB
        gl_FragColor = vec4(texture2D(yTexture, vTexCoord).p,
            texture2D(uTexture, vTexCoord).p,
            texture2D(vTexture, vTexCoord).p,
            1.0) * colorTransform;
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QGLShader::Fragment, shader);
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
    QnAbstractYv12ToRgbShaderProgram(context, parent),
    m_gammaStr(lit("y"))
{
    if (!final)
        return;
    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexCoord;

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
        float y = texture(yTexture, vTexCoord).p;
        gl_FragColor = vec4(clamp(pow(max(y+ yLevels2, 0.0) * yLevels1, yGamma), 0.0, 1.0),
            texture(uTexture, vTexCoord).p,
            texture(vTexture, vTexCoord).p,
            1.0) * colorTransform;
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif
    addShaderFromSourceCode(QGLShader::Fragment, shader);
    link();
}


// ============================= QnYv12ToRgbWithFisheyeShaderProgram ==================

// ---------------------------- QnFisheyeRectilinearProgram ------------------------------------

QnFisheyeRectilinearProgram::QnFisheyeRectilinearProgram(const QGLContext *context, QObject *parent, const QString& gammaStr):
    QnFisheyeShaderProgram(context, parent)
{
    setGammaStr(gammaStr);
}

QString QnFisheyeRectilinearProgram::getShaderText()
{
    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexCoord;

    uniform sampler2D yTexture;
    uniform sampler2D uTexture;
    uniform sampler2D vTexture;
    uniform float opacity;
    uniform float xShift;
    uniform float yShift;
    uniform float fovRot;
    uniform float dstFov;
    uniform float aspectRatio;
    uniform float yPos;
    uniform float xCenter;
    uniform float yCenter;
    uniform float radius;
    uniform float yLevels1;
    uniform float yLevels2;
    uniform float yGamma;
    uniform float maxX;
    uniform float maxY;
    uniform float xStretch;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0.0,  0.0,    0.0,    opacity);

    float kx =  2.0*tan(dstFov/2.0);

    // avoid function call for better shader compatibility
    vec3 xVect  = vec3(sin(xShift + PI/2.0), cos(xShift + PI/2.0), 0.0) * kx; 
    vec3 yVect  = vec3(cos(-yShift + PI/2.0) * sin(xShift), cos(-yShift + PI/2.0)*cos(xShift), sin(-yShift + PI/2.0)) * kx / xStretch;
    vec3 center = vec3(cos(-yShift) * sin(xShift), cos(-yShift)*cos(xShift), sin(-yShift));

    mat3 to3d = mat3(xVect.x,   yVect.x,    center.x,    
        xVect.y,   yVect.y,    center.y,
        xVect.z,   yVect.z,    center.z); // avoid transpose it is required glsl 1.2

    vec2 xy1 = vec2(1.0 / maxX, 1.0 / (maxY*aspectRatio));
    vec2 xy2 = vec2(-0.5,       -yPos/ aspectRatio);

    vec2 xy3 = vec2(maxX / PI * radius*2.0,  maxY / PI * radius*2.0*aspectRatio);
    vec2 xy4 = vec2(maxX * xCenter, maxY * yCenter);


    void main() 
    {
        vec3 pos3d = vec3(vTexCoord * xy1 + xy2, 1.0) * to3d; // point on the surface

        float theta = atan(pos3d.z, pos3d.x) + fovRot;            // fisheye angle
        float r     = acos(pos3d.y / length(pos3d));              // fisheye radius

        vec2 pos = vec2(cos(theta), sin(theta)) * r;
        pos = pos * xy3 + xy4;

        // do gamma correction and color transformation yuv->RGB
        float y = texture(yTexture, pos).p;
        if (all(bvec4(pos.x >= 0.0, pos.y >= 0.0, pos.x <= maxX, pos.y <= maxY)))
            gl_FragColor = vec4(%1,
            texture(uTexture, pos).p,
            texture(vTexture, pos).p,
            1.0) * colorTransform;
        else 
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    return QString(QLatin1String(shader)).arg(gammaStr());
}

// ------------------------- QnFisheyeEquirectangularHProgram -----------------------------

QnFisheyeEquirectangularHProgram::QnFisheyeEquirectangularHProgram(const QGLContext *context, QObject *parent, const QString& gammaStr)
    :QnFisheyeShaderProgram(context, parent)
{
    setGammaStr(gammaStr);
}

QString QnFisheyeEquirectangularHProgram::getShaderText()
{
    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexCoord;
    uniform sampler2D yTexture;
    uniform sampler2D uTexture;
    uniform sampler2D vTexture;
    uniform float opacity;
    uniform float xShift;
    uniform float yShift;
    uniform float fovRot;
    uniform float dstFov;
    uniform float aspectRatio;
    uniform float panoFactor;
    uniform float yPos;
    uniform float xCenter;
    uniform float yCenter;
    uniform float radius;
    uniform float yLevels1;
    uniform float yLevels2;
    uniform float yGamma;
    uniform float maxX;
    uniform float maxY;
    uniform float xStretch;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0.0,  0.0,    0.0,    opacity);

    mat3 perspectiveMatrix = mat3( vec3(1.0, 0.0,              0.0),
        vec3(0.0, cos(-fovRot), -sin(-fovRot)),
        vec3(0.0, sin(-fovRot),  cos(-fovRot)));

    vec2 xy1 = vec2(dstFov / maxX, (dstFov / panoFactor) / (maxY));
    vec2 xy2 = vec2(-0.5*dstFov,  -yPos*dstFov / panoFactor ) + vec2(xShift, 0.0);

    vec2 xy3 = vec2(maxX / PI * radius*2.0,  maxY / PI * radius*2.0*aspectRatio);
    vec2 xy4 = vec2(maxX * xCenter, maxY * yCenter);

    void main() 
    {
        vec2 pos = vTexCoord * xy1 + xy2;

        float cosTheta = cos(pos.x);
        float roty = -fovRot * cosTheta;
        float phi   = pos.y * (1.0 - roty*xy1.y)  - roty - yShift;
        float cosPhi = cos(phi);

        // Vector in 3D space
        vec3 psph = vec3(cosPhi * sin(pos.x),
            cosPhi * cosTheta,  // only one difference between H and V shaders: this 2 lines in back order
            sin(phi))  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        float theta   = atan(psph.z, psph.x);
        float r = acos(psph.y);

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r;

        // AR and non [0..1] range correction
        pos = pos * xy3 + xy4;

        // do gamma correction and color transformation yuv->RGB
        float y = texture(yTexture, pos).p;
        if (all(bvec4(pos.x >= 0.0, pos.y >= 0.0, pos.x <= maxX, pos.y <= maxY)))
            gl_FragColor = vec4(%1,
            texture(uTexture, pos).p,
            texture(vTexture, pos).p,
            1.0) * colorTransform;
        else 
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif
    
    return QString(QLatin1String(shader)).arg(gammaStr());
}

// ----------------------------------------- QnFisheyeEquirectangularVProgram ---------------------------------------

QnFisheyeEquirectangularVProgram::QnFisheyeEquirectangularVProgram(const QGLContext *context, QObject *parent, const QString& gammaStr)
    :QnFisheyeShaderProgram(context, parent)
{
    setGammaStr(gammaStr);
}

QString QnFisheyeEquirectangularVProgram::getShaderText()
{
    QByteArray shader(QN_SHADER_SOURCE(
        varying vec2 vTexCoord;
    uniform sampler2D yTexture;
    uniform sampler2D uTexture;
    uniform sampler2D vTexture;
    uniform float opacity;
    uniform float xShift;
    uniform float yShift;
    uniform float fovRot;
    uniform float dstFov;
    uniform float aspectRatio;
    uniform float panoFactor;
    uniform float yPos;
    uniform float xCenter;
    uniform float yCenter;
    uniform float radius;
    uniform float yLevels1;
    uniform float yLevels2;
    uniform float yGamma;
    uniform float maxX;
    uniform float maxY;
    uniform float xStretch;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0.0,  0.0,    0.0,    opacity);

    mat3 perspectiveMatrix = mat3( vec3(1.0, 0.0,              0.0),
        vec3(0.0, cos(-fovRot), -sin(-fovRot)),
        vec3(0.0, sin(-fovRot),  cos(-fovRot)));

    vec2 xy1 = vec2(dstFov / maxX, (dstFov / panoFactor) / (maxY));
    vec2 xy2 = vec2(-0.5*dstFov,  -yPos*dstFov / panoFactor ) + vec2(xShift, 0.0);

    vec2 xy3 = vec2(maxX / PI * radius*2.0,  maxY / PI * radius*2.0*aspectRatio);
    vec2 xy4 = vec2(maxX * xCenter, maxY * yCenter);

    void main() 
    {
        vec2 pos = vTexCoord * xy1 + xy2;

        float cosTheta = cos(pos.x);
        float roty = -fovRot * cosTheta;
        float phi   = -(pos.y * (1.0 - roty*xy1.y)  - roty - yShift);
        float cosPhi = cos(phi);

        // Vector in 3D space
        vec3 psph = vec3(cosPhi * sin(pos.x),
            sin(phi),  // only one difference between H and V shaders: this 2 lines in back order
            cosPhi * cosTheta)  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        float theta    = atan(psph.z, psph.x);
        float r  = acos(psph.y);

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r;

        // AR and non [0..1] range correction
        pos = pos * xy3 + xy4;

        // do gamma correction and color transformation yuv->RGB
        float y = texture(yTexture, pos).p;
        if (all(bvec4(pos.x >= 0.0, pos.y >= 0.0, pos.x <= maxX, pos.y <= maxY)))
            gl_FragColor = vec4(%1,
            texture(uTexture, pos).p,
            texture(vTexture, pos).p,
            1.0) * colorTransform;
        else 
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    return QString(QLatin1String(shader)).arg(gammaStr());
}

// ------------------------- QnAbstractRGBAShaderProgram ------------------------------

QnAbstractRGBAShaderProgram::QnAbstractRGBAShaderProgram(const QGLContext *context, QObject *parent, bool final):
    QnGLShaderProgram(context, parent) 
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

}

bool QnAbstractRGBAShaderProgram::link()
{
    bool rez = QnGLShaderProgram::link();
    if (rez) {
        m_rgbaTextureLocation = uniformLocation("rgbaTexture");
        m_opacityLocation = uniformLocation("opacity");
    }
    return rez;
}


// ---------------------------- QnFisheyeRGBRectilinearProgram ------------------------------------

QnFisheyeRGBRectilinearProgram::QnFisheyeRGBRectilinearProgram(const QGLContext *context, QObject *parent):
    QnFisheyeShaderProgram(context, parent)
{

}

QString QnFisheyeRGBRectilinearProgram::getShaderText()
{
    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexCoord;
    uniform sampler2D rgbaTexture;
    uniform float opacity;
    uniform float xShift;
    uniform float yShift;
    uniform float fovRot;
    uniform float dstFov;
    uniform float aspectRatio;
    uniform float yPos;
    uniform float xCenter;
    uniform float yCenter;
    uniform float radius;
    uniform float maxX;
    uniform float maxY;
    uniform float xStretch;

    const float PI = 3.1415926535;

    float kx =  2.0*tan(dstFov/2.0);

    // avoid function call for better shader compatibility
    vec3 xVect  = vec3(sin(xShift + PI/2.0), cos(xShift + PI/2.0), 0.0) * kx; 
    vec3 yVect  = vec3(cos(-yShift + PI/2.0) * sin(xShift), cos(-yShift + PI/2.0)*cos(xShift), sin(-yShift + PI/2.0)) * kx / xStretch;
    vec3 center = vec3(cos(-yShift) * sin(xShift), cos(-yShift)*cos(xShift), sin(-yShift));

    mat3 to3d = mat3(xVect.x,   yVect.x,    center.x,    
        xVect.y,   yVect.y,    center.y,
        xVect.z,   yVect.z,    center.z); // avoid transpose it is required glsl 1.2

    vec2 xy1 = vec2(1.0 / maxX, 1.0 / (maxY*aspectRatio));
    vec2 xy2 = vec2(-0.5,       -yPos/ aspectRatio);

    vec2 xy3 = vec2(maxX / PI * radius*2.0,  maxY / PI * radius*2.0*aspectRatio);
    vec2 xy4 = vec2(maxX * xCenter, maxY * yCenter);


    void main() 
    {
        vec3 pos3d = vec3(vTexCoord * xy1 + xy2, 1.0) * to3d; // point on the surface

        float theta = atan(pos3d.z, pos3d.x) + fovRot;            // fisheye angle
        float r     = acos(pos3d.y / length(pos3d));              // fisheye radius

        vec2 pos = vec2(cos(theta), sin(theta)) * r;
        pos = pos * xy3 + xy4;

        if (all(bvec4(pos.x >= 0.0, pos.y >= 0.0, pos.x <= maxX, pos.y <= maxY)))
            gl_FragColor = texture(rgbaTexture, pos);
        else 
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    return QLatin1String(shader);
}

// ------------------------- QnFisheyeRGBEquirectangularHProgram -----------------------------

QnFisheyeRGBEquirectangularHProgram::QnFisheyeRGBEquirectangularHProgram(const QGLContext *context, QObject *parent)
    :QnFisheyeShaderProgram(context, parent)
{

}

QString QnFisheyeRGBEquirectangularHProgram::getShaderText()
{
    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexCoord;
    uniform sampler2D rgbaTexture;
    uniform float opacity;
    uniform float xShift;
    uniform float yShift;
    uniform float fovRot;
    uniform float dstFov;
    uniform float aspectRatio;
    uniform float panoFactor;
    uniform float yPos;
    uniform float xCenter;
    uniform float yCenter;
    uniform float radius;
    uniform float maxX;
    uniform float maxY;
    uniform float xStretch;

    const float PI = 3.1415926535;

    mat3 perspectiveMatrix = mat3( vec3(1.0, 0.0,              0.0),
        vec3(0.0, cos(-fovRot), -sin(-fovRot)),
        vec3(0.0, sin(-fovRot),  cos(-fovRot)));

    vec2 xy1 = vec2(dstFov / maxX, (dstFov / panoFactor) / (maxY));
    vec2 xy2 = vec2(-0.5*dstFov,  -yPos*dstFov / panoFactor) + vec2(xShift, 0.0);

    vec2 xy3 = vec2(maxX / PI * radius*2.0,  maxY / PI * radius*2.0*aspectRatio);
    vec2 xy4 = vec2(maxX * xCenter, maxY * yCenter);

    void main() 
    {
        vec2 pos = vTexCoord * xy1 + xy2;

        float cosTheta = cos(pos.x);
        float roty = -fovRot * cosTheta;
        float phi   = pos.y * (1.0 - roty*xy1.y)  - roty - yShift;
        float cosPhi = cos(phi);

        // Vector in 3D space
        vec3 psph = vec3(cosPhi * sin(pos.x),
            cosPhi * cosTheta,  // only one difference between H and V shaders: this 2 lines in back order
            sin(phi))  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        float theta   = atan(psph.z, psph.x);
        float r = acos(psph.y);

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r;

        // AR and non [0..1] range correction
        pos = pos * xy3 + xy4;

        if (all(bvec4(pos.x >= 0.0, pos.y >= 0.0, pos.x <= maxX, pos.y <= maxY)))
            gl_FragColor = texture(rgbaTexture, pos);
        else 
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    return QLatin1String(shader);
}

// ----------------------------------------- QnFisheyeRGBEquirectangularVProgram ---------------------------------------

QnFisheyeRGBEquirectangularVProgram::QnFisheyeRGBEquirectangularVProgram(const QGLContext *context, QObject *parent)
    :QnFisheyeShaderProgram(context, parent)
{

}

QString QnFisheyeRGBEquirectangularVProgram::getShaderText()
{
    QByteArray shader(QN_SHADER_SOURCE(
        varying vec2 vTexCoord;
    uniform sampler2D rgbaTexture;
    uniform float opacity;
    uniform float xShift;
    uniform float yShift;
    uniform float fovRot;
    uniform float dstFov;
    uniform float aspectRatio;
    uniform float panoFactor;
    uniform float yPos;
    uniform float xCenter;
    uniform float yCenter;
    uniform float radius;
    uniform float maxX;
    uniform float maxY;
    uniform float xStretch;

    const float PI = 3.1415926535;

    mat3 perspectiveMatrix = mat3( vec3(1.0, 0.0, 0.0),
        vec3(0.0, cos(-fovRot), -sin(-fovRot)),
        vec3(0.0, sin(-fovRot),  cos(-fovRot)));

    vec2 xy1 = vec2(dstFov / maxX, (dstFov / panoFactor) / (maxY));
    vec2 xy2 = vec2(-0.5*dstFov,  -yPos*dstFov / panoFactor) + vec2(xShift, 0.0);

    vec2 xy3 = vec2(maxX / PI * radius*2.0,  maxY / PI * radius*2.0*aspectRatio);
    vec2 xy4 = vec2(maxX * xCenter, maxY * yCenter);

    void main() 
    {
        vec2 pos = vTexCoord * xy1 + xy2;

        float cosTheta = cos(pos.x);
        float roty = -fovRot * cosTheta;
        float phi   = -(pos.y * (1.0 - roty*xy1.y)  - roty - yShift);
        float cosPhi = cos(phi);

        // Vector in 3D space
        vec3 psph = vec3(cosPhi * sin(pos.x),
            sin(phi),  // only one difference between H and V shaders: this 2 lines in back order
            cosPhi * cosTheta)  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        float theta    = atan(psph.z, psph.x);
        float r  = acos(psph.y);

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r;

        // AR and non [0..1] range correction
        pos = pos * xy3 + xy4;

        if (all(bvec4(pos.x >= 0.0, pos.y >= 0.0, pos.x <= maxX, pos.y <= maxY)))
            gl_FragColor = texture(rgbaTexture, pos);
        else 
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    return QLatin1String(shader);
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
    QByteArray shader(QN_SHADER_SOURCE(
    varying vec2 vTexCoord;
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
        gl_FragColor = vec4(texture(yTexture, vTexCoord).p,
            texture(uTexture, vTexCoord).p-0.5,
            texture(vTexture, vTexCoord).p-0.5,
            texture(aTexture, vTexCoord).p) * colorTransform;
    }
    ));

#ifdef QT_OPENGL_ES_2
    shader =  QN_SHADER_SOURCE(precision mediump float;) + shader;
#endif

    addShaderFromSourceCode(QGLShader::Fragment, shader);

    link();
}