#include "yv12_to_rgb_shader_program.h"

QnAbstractYv12ToRgbShaderProgram::QnAbstractYv12ToRgbShaderProgram(const QGLContext *context, QObject *parent):
    QGLShaderProgram(context, parent) 
{
    addShaderFromSourceCode(QGLShader::Vertex, QN_SHADER_SOURCE(
        void main() {
            /*
            float a = 18.5 * (3.1415926 / 180.0);
            mat4 xRot = mat4(   1.0, 0.0,    0.0,     0.0,
                                0,   cos(a), -sin(a), 0.0,
                                0,   sin(a), cos(a),  0.0,
                                0,   0.0,    0.0,     1.0);
            
            vec4 v = vec4(gl_Vertex) * xRot;
            v.z = 500;
            */

            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

            gl_Position.z = gl_Position.z * 2;

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
    uniform float extraParam;

    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
                                1.0, -0.344, -0.714,  0.529,
                                1.0,  1.772,  0.0,   -0.886,
                                0.0,  0.0,    0.0,    opacity);

    const float PI = 3.1415926535;
    const float FOV = 180.0 * (PI / 180.0);
    const float ASPECT_RATIO = 4.0 / 3.0;

    void main() 
    {

        float PERSPECTIVE_ANGLE = 22.5 *  (PI / 180.0);
        mat3 perspectiveMatrix = mat3( 1.0, 0.0,                    0.0,
                                       0.0, cos(PERSPECTIVE_ANGLE), -sin(PERSPECTIVE_ANGLE),
                                       0.0, sin(PERSPECTIVE_ANGLE), cos(PERSPECTIVE_ANGLE));


        vec2 pos = gl_TexCoord[0].st -0.5; // go to coordinates in range [-1..+1]
        pos = pos * 0.5; // zoom

        const float F =  1.0 / PI;
        float theta = extraParam *PI + atan(pos.x, F);
        float phi = atan(pos.y, F) - PERSPECTIVE_ANGLE;

        // Vector in 3D space
        vec3 psph = vec3(cos(phi) * sin(theta),
                         cos(phi) * cos(theta),
                         sin(phi)             ) * perspectiveMatrix;


        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
        phi = atan(length(psph.xz),psph.y);
        float r = phi / FOV;

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
    m_extraParamLocation = uniformLocation("extraParam");
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
    uniform float extraParam;

    mat4 colorTransform = mat4( 1.0,  0.0,    1.402, -0.701,
        1.0, -0.344, -0.714,  0.529,
        1.0,  1.772,  0.0,   -0.886,
        0.0,  0.0,    0.0,    opacity);

    const float PI = 3.1415926535;
    const float FOV = 180.0 * (PI / 180.0);
    const float F =  0.5;
    const float ASPECT_RATIO = 4.0 / 3.0;

    void main() 
    {

        float PERSPECTIVE_ANGLE = 22.5 *  (PI / 180.0);
        mat3 perspectiveMatrix = mat3( 1.0, 0.0,                    0.0,
                                       0.0, cos(PERSPECTIVE_ANGLE), -sin(PERSPECTIVE_ANGLE),
                                       0.0, sin(PERSPECTIVE_ANGLE), cos(PERSPECTIVE_ANGLE));


        vec2 pos = gl_TexCoord[0].st -0.5; // go to coordinates in range [-1..+1]
        pos = pos * 0.5; // zoom

        float theta = (pos.x) * FOV + extraParam;
        float phi = (pos.y) * FOV - PERSPECTIVE_ANGLE;

        // Vector in 3D space
        vec3 psph = vec3(cos(phi) * sin(theta),
                         cos(phi) * cos(theta),
                         sin(phi)             )  * perspectiveMatrix;


        // Calculate fisheye angle and radius
        theta = atan(psph.z, psph.x);
        phi = atan(length(psph.xz),psph.y);
        float r = phi / FOV;

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
    m_extraParamLocation = uniformLocation("extraParam");
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
