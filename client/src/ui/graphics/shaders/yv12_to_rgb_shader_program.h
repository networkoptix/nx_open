#ifndef QN_YV12_TO_RGB_SHADER_PROGRAM_H
#define QN_YV12_TO_RGB_SHADER_PROGRAM_H

#include "arb_shader_program.h"

class QnYv12ToRgbShaderProgram: public QGLShaderProgram {
    Q_OBJECT;
public:
    QnYv12ToRgbShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    void setYTexture(int target) {
        setUniformValue(m_yTextureLocation, target);
    }
    void setUTexture(int target) {
        setUniformValue(m_uTextureLocation, target);
    }
    void setVTexture(int target) {
        setUniformValue(m_vTextureLocation, target);
    }

    void setOpacity(GLfloat opacity) {
        setUniformValue(m_opacityLocation, opacity);
    }

    void setGamma1(GLfloat gamma1) {
        setUniformValue(m_yGamma1Location, gamma1);
    }

    void setGamma2(GLfloat gamma2) {
        setUniformValue(m_yGamma2Location, gamma2);
    }
private:
    int m_yTextureLocation;
    int m_uTextureLocation;
    int m_vTextureLocation;
    int m_opacityLocation;
    int m_yGamma1Location;
    int m_yGamma2Location;
};

class QnYv12ToRgbaShaderProgram: public QGLShaderProgram {
    Q_OBJECT;
public:
    QnYv12ToRgbaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    void setYTexture(int target) {
        setUniformValue(m_yTextureLocation, target);
    }
    void setUTexture(int target) {
        setUniformValue(m_uTextureLocation, target);
    }
    void setVTexture(int target) {
        setUniformValue(m_vTextureLocation, target);
    }
    void setATexture(int target) {
        setUniformValue(m_aTextureLocation, target);
    }
    void setOpacity(GLfloat opacity) {
        setUniformValue(m_opacityLocation, opacity);
    }

private:
    int m_yTextureLocation;
    int m_uTextureLocation;
    int m_vTextureLocation;
    int m_aTextureLocation;
    int m_opacityLocation;
};

#endif // QN_YV12_TO_RGB_SHADER_PROGRAM_H
