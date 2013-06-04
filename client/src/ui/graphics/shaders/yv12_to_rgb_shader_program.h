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

    bool isValid() const { return m_isValid; }
private:
    int m_yTextureLocation;
    int m_uTextureLocation;
    int m_vTextureLocation;
    int m_opacityLocation;
    bool m_isValid;
};

class QnYv12ToRgbaShaderProgram: public QnArbShaderProgram {
    Q_OBJECT;
public:
    QnYv12ToRgbaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    void setParameters(GLfloat brightness, GLfloat contrast, GLfloat hue, GLfloat saturation, GLfloat opacity);
};

#endif // QN_YV12_TO_RGB_SHADER_PROGRAM_H
