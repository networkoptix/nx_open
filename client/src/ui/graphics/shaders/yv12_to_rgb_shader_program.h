#ifndef QN_YV12_TO_RGB_SHADER_PROGRAM_H
#define QN_YV12_TO_RGB_SHADER_PROGRAM_H

#include "utils/color_space/image_correction.h"
#include <QtOpenGL/QGLShaderProgram>
#include <QtOpenGL/QtOpenGL>
#include "shader_source.h"
#include "ui/fisheye/fisheye_ptz_processor.h"

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

    virtual bool link() override; 
private:
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
    QnYv12ToRgbWithGammaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL, bool final = true);

    void setImageCorrection(const ImageCorrectionResult& value)
    {
        setUniformValue(m_yLevels1Location, value.aCoeff);
        setUniformValue(m_yLevels2Location, value.bCoeff);
        setUniformValue(m_yGammaLocation, value.gamma);
    }
    virtual bool link() override;
private:
    int m_yLevels1Location;
    int m_yLevels2Location;
    int m_yGammaLocation;
};

class QnYv12ToRgbWithFisheyeShaderProgram : public QnYv12ToRgbWithGammaShaderProgram
{
public:
    QnYv12ToRgbWithFisheyeShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL, const QString& gammaStr = lit("y"));
    
    void setDevorpingParams(const DevorpingParams& params)
    {
        setUniformValue(m_xShiftLocation, (float) params.xAngle);
        setUniformValue(m_yShiftLocation, (float) params.yAngle);
        setUniformValue(m_perspShiftLocation, (float) params.pAngle);
        setUniformValue(m_dstFovLocation, (float) params.fov);
        setUniformValue(m_aspectRatioLocation, (float) params.aspectRatio);
    }

    virtual bool link() override;
protected:
    int m_xShiftLocation;
    int m_yShiftLocation;
    int m_perspShiftLocation;
    int m_dstFovLocation;
    int m_aspectRatioLocation;
};

class QnYv12ToRgbWithFisheyeAndGammaShaderProgram : public QnYv12ToRgbWithFisheyeShaderProgram
{
public:
    QnYv12ToRgbWithFisheyeAndGammaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);
};

class QnYv12ToRgbaShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT;
public:
    QnYv12ToRgbaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);

    virtual bool link() override;

    void setATexture(int target) {
        setUniformValue(m_aTextureLocation, target);
    }
private:
    int m_aTextureLocation;
};

#endif // QN_YV12_TO_RGB_SHADER_PROGRAM_H
