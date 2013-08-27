#ifndef QN_ABSTRACT_PTZ_CONTROLLER_H
#define QN_ABSTRACT_PTZ_CONTROLLER_H

#include <QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <utils/math/math.h>

namespace {
    qreal gradToRad(qreal x) { return x * M_PI / 180.0; }
    qreal radToGrad(qreal x) { return x * 180.0 / M_PI; }

    /**
     * \param fovDegreees               Width-based FOV in degrees.
     * \returns                         Width-based 35mm-equivalent focal length.
     */
    qreal fovTo35mmEquiv(qreal fovRad) {
        return (36.0 / 2.0) / std::tan((fovRad / 2.0));
    }

    /**
     * \param mm35Equiv                 Width-based 35mm-equivalent focal length.
     * \returns                         Width-based FOV in degrees.
     */
    qreal mm35EquivToFov(qreal mm35Equiv) {
        return std::atan((36.0 / 2.0) / mm35Equiv) * 2.0;
    }

} // anonymous namespace


class QnPtzSpaceMapper;

class QnAbstractPtzController: public QObject {
    Q_OBJECT
public:
    QnAbstractPtzController(QnResource* resource);
    virtual ~QnAbstractPtzController();

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) = 0;
    virtual int stopMove() = 0;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) = 0;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) = 0;
    virtual Qn::PtzCapabilities getCapabilities() = 0;
    virtual const QnPtzSpaceMapper *getSpaceMapper() = 0;

    //bool calibrate(qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff);
    //void getCalibrate(qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff);

    static bool calibrate(QnVirtualCameraResourcePtr res, qreal xVelocityCoeff, qreal yVelocityCoeff, qreal zoomVelocityCoeff);
    static void getCalibrate(QnVirtualCameraResourcePtr res, qreal &xVelocityCoeff, qreal &yVelocityCoeff, qreal &zoomVelocityCoeff);

    qreal getXVelocityCoeff() const;
    qreal getYVelocityCoeff() const;
    qreal getZoomVelocityCoeff() const;

    virtual bool isEnabled() const { return true; }
    virtual void setEnabled(bool enabled) { Q_UNUSED(enabled); }

protected:
    QnMediaResource* m_resource;
};

class QnVirtualPtzController: public QnAbstractPtzController {
    Q_OBJECT
public:
    QnVirtualPtzController(QnResource* resource): QnAbstractPtzController(resource), m_animationEnabled(false) {}
    
    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool animationEnabled) { m_animationEnabled = animationEnabled; }

    virtual void changePanoMode() = 0;
    virtual QString getPanoModeText() const = 0;
private:
    bool m_animationEnabled;
};

#endif // QN_ABSTRACT_PTZ_CONTROLLER_H
