#ifndef QN_ABSTRACT_PTZ_CONTROLLER_H
#define QN_ABSTRACT_PTZ_CONTROLLER_H

#include <QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <utils/math/math.h>

namespace Qn {
    enum PtzCapability
    {
        NoPtzCapabilities                   = 0x00,
        AbsolutePtzCapability               = 0x020,
        ContinuousPanTiltCapability         = 0x040,
        ContinuousZoomCapability            = 0x080,
        OctagonalPtzCapability              = 0x100, // TODO: #Elric deprecate this shit. Not really a capability.

        /* Shortcuts */
        AllPtzCapabilities                  = AbsolutePtzCapability | ContinuousPanTiltCapability | ContinuousZoomCapability | OctagonalPtzCapability,

        /* Deprecated capabilities. */
        DeprecatedContinuousPtzCapability   = 0x001,
        DeprecatedZoomCapability            = 0x002,
    };
    Q_DECLARE_FLAGS(PtzCapabilities, PtzCapability);
    Q_DECLARE_OPERATORS_FOR_FLAGS(PtzCapabilities);

    /**
     * \param capabilities              Camera capability flags containing some deprecated values.
     * \returns                         Camera capability flags with deprecated values replaced with new ones.
     */
    inline Qn::PtzCapabilities undeprecatePtzCapabilities(Qn::PtzCapabilities capabilities) 
    {
        Qn::PtzCapabilities result = capabilities;

        if(result & Qn::DeprecatedContinuousPtzCapability) {
            result &= ~Qn::DeprecatedContinuousPtzCapability;
            result |= Qn::ContinuousPanTiltCapability | Qn::ContinuousZoomCapability;
        }

        if(result & Qn::DeprecatedZoomCapability) {
            result &= ~Qn::DeprecatedZoomCapability;
            result |= Qn::ContinuousZoomCapability;
        }

        return result;
    }
};

namespace 
{
    qreal gradToRad(qreal x) { return x * M_PI / 180.0; }
    qreal radToGrad(qreal x)   { return x * 180.0 / M_PI; }

    /**
     * \param fovDegreees               Width-based FOV in degrees.
     * \returns                         Width-based 35mm-equivalent focal length.
     */
    qreal fovTo35mmEquiv(qreal fovRad) {
        return (36.0 / 2.0) / std::tan((fovRad / 2.0));
    }

    qreal mm35vToFov(qreal mm) {
        return std::atan((36.0 / 2.0) / mm) * 2.0;
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
    virtual void setEnabled(bool ) {}

protected:
    QnMediaResource* m_resource;
};

class QnVirtualPtzController: public QnAbstractPtzController
{
public:
    QnVirtualPtzController(QnResource* resource): QnAbstractPtzController(resource), m_animate(false) {}
    void setAnimationEnabled(bool value) { m_animate = value; }
protected:
    bool m_animate;
};

#endif // QN_ABSTRACT_PTZ_CONTROLLER_H
