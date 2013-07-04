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
        uniform float yShift;
        uniform float perspShift;
        uniform float dstFov;
        uniform float aspectRatio;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                1.0, -0.344, -0.714,  0.529,
                                1.0,  1.772,  0.0,   -0.886,
                                0.0,  0.0,    0.0,    opacity);
    mat3 perspectiveMatrix = mat3( 1.0, 0.0,              0.0,
                                   0.0, cos(perspShift), -sin(perspShift),
                                   0.0, sin(perspShift),  cos(perspShift));

    vec3 sphericalToCartesian(float theta, float phi) {
        return vec3(cos(phi) * sin(theta), cos(phi)*cos(theta), sin(phi));
    }

    vec3 center = sphericalToCartesian(xShift, -yShift);
    vec3 x  = sphericalToCartesian(xShift + PI/2.0, 0.0) * 2.0*tan(dstFov/2.0);
    vec3 y  = sphericalToCartesian(xShift, -yShift + PI/2.0) * 2.0*tan(dstFov/2.0);

    void main() 
    {
        
        vec2 pos = gl_TexCoord[0].st - 0.5; // go to coordinates in range [-dstFov/2...+dstFov/2]
        pos.y = pos.y / aspectRatio;

        vec3 pos3d = center + x * pos.x + y * pos.y;

        float theta = atan(pos3d.x, pos3d.y);
        float phi   = asin(pos3d.z / length(pos3d));
        
        // Vector in 3D space
        vec3 psph = sphericalToCartesian(theta, phi) * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
        phi   = atan(length(psph.xz), psph.y);
        float r = phi / PI; // fisheye FOV

        // return from polar coordinates
        pos = vec2(cos(theta), sin(theta)) * r + 0.5;
        
        //vec2 pos = gl_TexCoord[0].st;

        // do color transformation yuv->RGB
        gl_FragColor = vec4(texture2D(yTexture, pos).p,
                            texture2D(uTexture, pos).p,
                            texture2D(vTexture, pos).p,
                            1.0) * colorTransform;
    }
    ));

    link();

    m_yTextureLocation = uniformLocation("yTexture");
    m_uTextureLocation = uniformLocation("uTexture");
    m_vTextureLocation = uniformLocation("vTexture");
    m_opacityLocation = uniformLocation("opacity");
    m_xShiftLocation = uniformLocation("xShift");
    m_yShiftLocation = uniformLocation("yShift");
    m_perspShiftLocation = uniformLocation("perspShift");
    m_dstFovLocation = uniformLocation("dstFov");
    m_aspectRatioLocation = uniformLocation("aspectRatio");
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
        uniform float yShift;
        uniform float perspShift;
        uniform float dstFov;
        uniform float aspectRatio;

    const float PI = 3.1415926535;
    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                1.0, -0.344, -0.714,  0.529,
                                1.0,  1.772,  0.0,   -0.886,
                                0.0,  0.0,    0.0,    opacity);
    mat3 perspectiveMatrix = mat3( 1.0, 0.0,              0.0,
                                   0.0, cos(perspShift), -sin(perspShift),
                                   0.0, sin(perspShift),  cos(perspShift));

    void main() 
    {
        vec2 pos = (gl_TexCoord[0].st -0.5) * dstFov; // go to coordinates in range [-dstFov/2..+dstFov/2]

        float theta = pos.x + xShift;
        float phi   = pos.y/aspectRatio - yShift - perspShift;

        // Vector in 3D space
        vec3 psph = vec3(cos(phi) * sin(theta),
                         cos(phi) * cos(theta),
                         sin(phi)             )  * perspectiveMatrix;

        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
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

    link();

    m_yTextureLocation = uniformLocation("yTexture");
    m_uTextureLocation = uniformLocation("uTexture");
    m_vTextureLocation = uniformLocation("vTexture");
    m_opacityLocation = uniformLocation("opacity");
    m_yLevels1Location = uniformLocation("yLevels1");
    m_yLevels2Location = uniformLocation("yLevels2");
    m_yGammaLocation = uniformLocation("yGamma");
    m_xShiftLocation = uniformLocation("xShift");
    m_yShiftLocation = uniformLocation("yShift");
    m_perspShiftLocation = uniformLocation("perspShift");
    m_dstFovLocation = uniformLocation("dstFov");
    m_aspectRatioLocation = uniformLocation("aspectRatio");
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
