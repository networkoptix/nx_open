#ifndef QN_YV12_TO_RGB_SHADER_PROGRAM_H
#define QN_YV12_TO_RGB_SHADER_PROGRAM_H

#include "arb_shader_program.h"
#include "utils/color_space/image_correction.h"

class QnAbstractYv12ToRgbShaderProgram : public QGLShaderProgram 
{
public:
    QnAbstractYv12ToRgbShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

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

protected:
    int m_yTextureLocation;
    int m_uTextureLocation;
    int m_vTextureLocation;
    int m_opacityLocation;
};

class QnYv12ToRgbShaderProgram: public QnAbstractYv12ToRgbShaderProgram 
{
public:
    QnYv12ToRgbShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);
};

class QnYv12ToRgbWithGammaShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT;
public:
    QnYv12ToRgbWithGammaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    void setImageCorrection(const ImageCorrectionResult& value)
    {
        setUniformValue(m_yLevels1Location, value.aCoeff);
        setUniformValue(m_yLevels2Location, value.bCoeff);
        setUniformValue(m_yGammaLocation, value.gamma);
    }
private:
    int m_yLevels1Location;
    int m_yLevels2Location;
    int m_yGammaLocation;
};


class QnYv12ToRgbaShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT;
public:
    QnYv12ToRgbaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    void setATexture(int target) {
        setUniformValue(m_aTextureLocation, target);
    }
private:
    int m_aTextureLocation;
};

#endif // QN_YV12_TO_RGB_SHADER_PROGRAM_H
