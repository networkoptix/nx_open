#ifndef QN_YV12_TO_RGB_SHADER_PROGRAM_H
#define QN_YV12_TO_RGB_SHADER_PROGRAM_H

#include "utils/color_space/image_correction.h"
#include <QtOpenGL/QGLShaderProgram>
#include <QtOpenGL/QtOpenGL>
#include "shader_source.h"
#include "ui/fisheye/fisheye_ptz_controller.h"

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

class QnFisheyeShaderProgram : public QnYv12ToRgbWithGammaShaderProgram
{
public:
    QnFisheyeShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL, const QString& gammaStr = QString());
    
    void setDewarpingParams(const DewarpingParams& params, float aspectRatio, float maxX, float maxY)
    {
        //float fovRot = sin(params.xAngle)*params.fovRot;
        float fovRot = gradToRad(-12);
        if (params.horizontalView) {
            setUniformValue(m_yShiftLocation, (float) (params.yAngle));
            setUniformValue(m_yCenterLocation, (float) 0.5);
            setUniformValue(m_xShiftLocation, (float) params.xAngle);
            setUniformValue(m_fovRotLocation, (float) fovRot);
        }
        else {
            //setUniformValue(m_yShiftLocation, (float) params.yAngle);
            setUniformValue(m_yShiftLocation, (float) 0);
            setUniformValue(m_yCenterLocation, (float) 1.0);
            //setUniformValue(m_xShiftLocation, (float) fovRot);
            //setUniformValue(m_fovRotLocation, (float) -params.xAngle);
            setUniformValue(m_xShiftLocation, (float) params.xAngle);
            setUniformValue(m_fovRotLocation, (float) -params.yAngle);
        }
        setUniformValue(m_aspectRatioLocation, aspectRatio);
        setUniformValue(m_dstFovLocation, (float) params.fov);

        setUniformValue(m_maxXLocation, maxX);
        setUniformValue(m_maxYLocation, maxY);
    }

    virtual bool link() override;
private:
    QString getShaderText();
protected:
    int m_xShiftLocation;
    int m_yShiftLocation;
    int m_fovRotLocation;
    int m_dstFovLocation;
    int m_aspectRatioLocation;
    int m_yCenterLocation;
    int m_maxXLocation;
    int m_maxYLocation;
};

class QnFisheyeWithGammaShaderProgram : public QnFisheyeShaderProgram
{
public:
    QnFisheyeWithGammaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);
private:
    QString getShaderText();
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
