#ifndef QN_YV12_TO_RGB_SHADER_PROGRAM_H
#define QN_YV12_TO_RGB_SHADER_PROGRAM_H

#include "utils/color_space/image_correction.h"
#include <QtOpenGL/QGLShaderProgram>
#include <QtOpenGL/QtOpenGL>
#include "shader_source.h"

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

class QnAbstractYv12ToRgbShaderProgram : public QGLShaderProgram {
    Q_OBJECT
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


class QnAbstractRGBAShaderProgram : public QGLShaderProgram {
    Q_OBJECT
public:
    QnAbstractRGBAShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL, bool final = true);

    void setRGBATexture(int target) {
        setUniformValue(m_rgbaTextureLocation, target);
    }

    void setOpacity(GLfloat opacity) {
        setUniformValue(m_opacityLocation, opacity);
    }

    virtual bool link() override;

private:
    int m_rgbaTextureLocation;
    int m_opacityLocation;
};

class QnYv12ToRgbShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT
public:
    QnYv12ToRgbShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL);
};


class QnYv12ToRgbWithGammaShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT
public:
    QnYv12ToRgbWithGammaShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL, bool final = true);

    void setImageCorrection(const ImageCorrectionResult& value) {
        setUniformValue(m_yLevels1Location, value.aCoeff);
        setUniformValue(m_yLevels2Location, value.bCoeff);
        setUniformValue(m_yGammaLocation, value.gamma);
    }
    virtual bool link() override;

    void setGammaStr(const QString& value) {m_gammaStr = value; }
    QString gammaStr() const { return m_gammaStr; }
private:
    int m_yLevels1Location;
    int m_yLevels2Location;
    int m_yGammaLocation;
    QString m_gammaStr;
};

template <class T>
class QnFisheyeShaderProgram : public T
{
public:
    QnFisheyeShaderProgram(const QGLContext *context = NULL, QObject *parent = NULL): T(context, parent, false) {}
    
    void setDewarpingParams(const QnMediaDewarpingParams &mediaParams,
                            const QnItemDewarpingParams &itemParams,
                            float aspectRatio, float maxX, float maxY)
    {
        if (itemParams.panoFactor == 1)
        {
            float fovRot = sin(itemParams.xAngle) * qDegreesToRadians(mediaParams.fovRot);
            if (mediaParams.viewMode == QnMediaDewarpingParams::Horizontal) {
                T::setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle));
                T::setUniformValue(m_yPos, (float) 0.5);
                T::setUniformValue(m_xShiftLocation, (float) itemParams.xAngle);
                T::setUniformValue(m_fovRotLocation, (float) fovRot);
            }
            else {
                T::setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle - M_PI/2.0 - itemParams.fov/2.0));
                T::setUniformValue(m_yPos, (float) 1.0);
                T::setUniformValue(m_xShiftLocation, (float) fovRot);
                T::setUniformValue(m_fovRotLocation, (float) -itemParams.xAngle);
            }
        }
        else {
            T::setUniformValue(m_xShiftLocation, (float) itemParams.xAngle);
            T::setUniformValue(m_fovRotLocation, (float) qDegreesToRadians(mediaParams.fovRot));
            //setUniformValue(m_fovRotLocation, (float) gradToRad(-11.0));
            if (mediaParams.viewMode == QnMediaDewarpingParams::Horizontal) {
                T::setUniformValue(m_yPos, (float) 0.5);
                T::setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle));
            }
            else {
                T::setUniformValue(m_yPos, (float) 1.0);
                T::setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle - itemParams.fov/itemParams.panoFactor/2.0));
            }
        }

        T::setUniformValue(m_aspectRatioLocation, (float) (aspectRatio));
        T::setUniformValue(m_panoFactorLocation, (float) (itemParams.panoFactor));
        T::setUniformValue(m_dstFovLocation, (float) itemParams.fov);

        T::setUniformValue(m_yCenterLocation, (float) mediaParams.yCenter);
        T::setUniformValue(m_xCenterLocation, (float) mediaParams.xCenter);
        T::setUniformValue(m_radiusLocation, (float) mediaParams.radius);
        T::setUniformValue(m_maxXLocation, maxX);
        T::setUniformValue(m_maxYLocation, maxY);
    }

    virtual bool link() override {
        T::addShaderFromSourceCode(QGLShader::Fragment, getShaderText());
        bool rez = T::link();
        if (rez) {
            m_xShiftLocation = T::uniformLocation("xShift");
            m_yShiftLocation = T::uniformLocation("yShift");
            m_fovRotLocation = T::uniformLocation("fovRot");
            m_dstFovLocation = T::uniformLocation("dstFov");
            m_aspectRatioLocation = T::uniformLocation("aspectRatio");
            m_panoFactorLocation = T::uniformLocation("panoFactor");
            m_yPos = T::uniformLocation("yPos");
            m_xCenterLocation = T::uniformLocation("xCenter");
            m_yCenterLocation = T::uniformLocation("yCenter");
            m_radiusLocation = T::uniformLocation("radius");

            m_maxXLocation = T::uniformLocation("maxX");
            m_maxYLocation = T::uniformLocation("maxY");
        }
        return rez;
    }

protected:
    virtual QString getShaderText() = 0;
protected:
    int m_xShiftLocation;
    int m_yShiftLocation;
    int m_fovRotLocation;
    int m_dstFovLocation;
    int m_aspectRatioLocation;
    int m_panoFactorLocation;
    
    int m_yPos;
    int m_xCenterLocation;
    int m_radiusLocation;
    int m_yCenterLocation;
    
    int m_maxXLocation;
    int m_maxYLocation;
};

// --------- fisheye YUV (with optional gamma) ---------------

class QnFisheyeRectilinearProgram : public QnFisheyeShaderProgram<QnYv12ToRgbWithGammaShaderProgram>
{
	Q_OBJECT
public:
    QnFisheyeRectilinearProgram(const QGLContext *context = NULL, QObject *parent = NULL, const QString& gammaStr = lit("y"));
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeEquirectangularHProgram : public QnFisheyeShaderProgram<QnYv12ToRgbWithGammaShaderProgram>
{
	Q_OBJECT
public:
    QnFisheyeEquirectangularHProgram(const QGLContext *context = NULL, QObject *parent = NULL, const QString& gammaStr = lit("y"));
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeEquirectangularVProgram : public QnFisheyeShaderProgram<QnYv12ToRgbWithGammaShaderProgram>
{
	Q_OBJECT
public:
    QnFisheyeEquirectangularVProgram(const QGLContext *context = NULL, QObject *parent = NULL, const QString& gammaStr = lit("y"));
protected:
    virtual QString getShaderText() override;
};

// --------- fisheye RGB ---------------

class QnFisheyeRGBRectilinearProgram : public QnFisheyeShaderProgram<QnAbstractRGBAShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeRGBRectilinearProgram(const QGLContext *context = NULL, QObject *parent = NULL);
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeRGBEquirectangularHProgram : public QnFisheyeShaderProgram<QnAbstractRGBAShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeRGBEquirectangularHProgram(const QGLContext *context = NULL, QObject *parent = NULL);
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeRGBEquirectangularVProgram : public QnFisheyeShaderProgram<QnAbstractRGBAShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeRGBEquirectangularVProgram(const QGLContext *context = NULL, QObject *parent = NULL);
protected:
    virtual QString getShaderText() override;
};

// ---------  RGBA programm ---------------

class QnYv12ToRgbaShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT
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
