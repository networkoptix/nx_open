#ifndef QN_YV12_TO_RGB_SHADER_PROGRAM_H
#define QN_YV12_TO_RGB_SHADER_PROGRAM_H



#include "utils/color_space/image_correction.h"
#include <QtOpenGL/QGLShaderProgram>
#include <QtOpenGL/QtOpenGL>
#include "shader_source.h"

#include <core/ptz/item_dewarping_params.h>
#include <core/ptz/media_dewarping_params.h>

#include "base_shader_program.h"

class QnAbstractYv12ToRgbShaderProgram : public QnGLShaderProgram {
    Q_OBJECT
public:
    QnAbstractYv12ToRgbShaderProgram(QObject *parent = NULL);

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
    bool wasLinked(){ return m_wasLinked; };
private:
    bool m_wasLinked;
    int m_yTextureLocation;
    int m_uTextureLocation;
    int m_vTextureLocation;
    int m_opacityLocation;
};


class QnAbstractRGBAShaderProgram : public QnGLShaderProgram {
    Q_OBJECT
public:
    QnAbstractRGBAShaderProgram(QObject *parent = NULL, bool final = true);

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
    QnYv12ToRgbShaderProgram(QObject *parent = NULL);
};


class QnYv12ToRgbWithGammaShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT
public:
    QnYv12ToRgbWithGammaShaderProgram(QObject *parent = NULL, bool final = true);

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

template <class Base>
class QnFisheyeShaderProgram : public Base {
    typedef Base base_type;
public:
    QnFisheyeShaderProgram(QObject *parent = NULL)
        : Base(parent, false),
          m_add_shader(false)
    {
    }

    using base_type::uniformLocation;
    using base_type::setUniformValue;
    using base_type::addShaderFromSourceCode;

    void setDewarpingParams(const QnMediaDewarpingParams &mediaParams,
                            const QnItemDewarpingParams &itemParams,
                            float aspectRatio, float maxX, float maxY)
    {
        if (itemParams.panoFactor == 1)
        {
            float fovRot = sin(itemParams.xAngle) * qDegreesToRadians(mediaParams.fovRot);
            if (mediaParams.viewMode == QnMediaDewarpingParams::Horizontal) {
                setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle));
                setUniformValue(m_yPos, (float) 0.5);
                setUniformValue(m_xShiftLocation, (float) itemParams.xAngle);
                setUniformValue(m_fovRotLocation, (float) fovRot);
            }
            else {
                setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle - M_PI/2.0 - itemParams.fov/2.0/aspectRatio));
                setUniformValue(m_yPos, (float) 1.0);
                setUniformValue(m_xShiftLocation, (float) fovRot);
                setUniformValue(m_fovRotLocation, (float) -itemParams.xAngle);
            }
        }
        else {
            setUniformValue(m_xShiftLocation, (float) itemParams.xAngle);
            setUniformValue(m_fovRotLocation, (float) qDegreesToRadians(mediaParams.fovRot));
            //setUniformValue(m_fovRotLocation, (float) gradToRad(-11.0));
            if (mediaParams.viewMode == QnMediaDewarpingParams::Horizontal) {
                setUniformValue(m_yPos, (float) 0.5);
                setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle));
            }
            else {
                setUniformValue(m_yPos, (float) 1.0);
                setUniformValue(m_yShiftLocation, (float) (itemParams.yAngle - itemParams.fov/itemParams.panoFactor/2.0));
            }
        }

        setUniformValue(m_aspectRatioLocation, (float) (aspectRatio / mediaParams.hStretch));
        setUniformValue(m_panoFactorLocation, (float) (itemParams.panoFactor));
        setUniformValue(m_dstFovLocation, (float) itemParams.fov);

        setUniformValue(m_yCenterLocation, (float) mediaParams.yCenter);
        setUniformValue(m_xCenterLocation, (float) mediaParams.xCenter);
        setUniformValue(m_radiusLocation, (float) mediaParams.radius);
        setUniformValue(m_maxXLocation, maxX);
        setUniformValue(m_maxYLocation, maxY);
        setUniformValue(m_xStretchLocation, (float) mediaParams.hStretch);
    }

    virtual bool link() override {
        if ( !m_add_shader )
        {
            addShaderFromSourceCode(QOpenGLShader::Fragment, getShaderText());
            m_add_shader = true;
        }        
        bool rez = base_type::link();
        if (rez) {
            m_xShiftLocation = uniformLocation("xShift");
            m_yShiftLocation = uniformLocation("yShift");
            m_fovRotLocation = uniformLocation("fovRot");
            m_dstFovLocation = uniformLocation("dstFov");
            m_aspectRatioLocation = uniformLocation("aspectRatio");
            m_panoFactorLocation = uniformLocation("panoFactor");
            m_yPos = uniformLocation("yPos");
            m_xCenterLocation = uniformLocation("xCenter");
            m_yCenterLocation = uniformLocation("yCenter");
            m_radiusLocation = uniformLocation("radius");

            m_maxXLocation = uniformLocation("maxX");
            m_maxYLocation = uniformLocation("maxY");
            m_xStretchLocation = uniformLocation("xStretch");
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
    int m_xStretchLocation;
    bool m_add_shader;
};

// --------- fisheye YUV (with optional gamma) ---------------

class QnFisheyeRectilinearProgram : public QnFisheyeShaderProgram<QnYv12ToRgbWithGammaShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeRectilinearProgram(QObject *parent = NULL, const QString& gammaStr = lit("y"));
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeEquirectangularHProgram : public QnFisheyeShaderProgram<QnYv12ToRgbWithGammaShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeEquirectangularHProgram(QObject *parent = NULL, const QString& gammaStr = lit("y"));
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeEquirectangularVProgram : public QnFisheyeShaderProgram<QnYv12ToRgbWithGammaShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeEquirectangularVProgram(QObject *parent = NULL, const QString& gammaStr = lit("y"));
protected:
    virtual QString getShaderText() override;
};

// --------- fisheye RGB ---------------

class QnFisheyeRGBRectilinearProgram : public QnFisheyeShaderProgram<QnAbstractRGBAShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeRGBRectilinearProgram(QObject *parent = NULL);
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeRGBEquirectangularHProgram : public QnFisheyeShaderProgram<QnAbstractRGBAShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeRGBEquirectangularHProgram(QObject *parent = NULL);
protected:
    virtual QString getShaderText() override;
};

class QnFisheyeRGBEquirectangularVProgram : public QnFisheyeShaderProgram<QnAbstractRGBAShaderProgram>
{
    Q_OBJECT
public:
    QnFisheyeRGBEquirectangularVProgram(QObject *parent = NULL);
protected:
    virtual QString getShaderText() override;
};

// ---------  RGBA programm ---------------

class QnYv12ToRgbaShaderProgram: public QnAbstractYv12ToRgbShaderProgram {
    Q_OBJECT
public:
    QnYv12ToRgbaShaderProgram(QObject *parent = NULL);

    virtual bool link() override;

    void setATexture(int target) {
        setUniformValue(m_aTextureLocation, target);
    }

private:
    int m_aTextureLocation;
};


#endif // QN_YV12_TO_RGB_SHADER_PROGRAM_H
