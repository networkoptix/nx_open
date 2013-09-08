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


struct QnPtzLimits {
    qreal minPan;
    qreal maxPan;
    qreal minTilt;
    qreal maxTilt;
    qreal minFov;
    qreal maxFov;
};

class QnAbstractPtzController: public QObject {
    Q_OBJECT
public:
    QnAbstractPtzController(QnResource *resource): m_resource(resource) {}
    virtual ~QnAbstractPtzController() {}

    virtual Qn::PtzCapabilities getCapabilities() = 0;

    virtual int startMove(qreal panSpeed, qreal tiltSpeed, qreal zoomSpeed) = 0;
    virtual int stopMove() = 0;

    virtual int setPhysicalPosition(qreal pan, qreal tilt, qreal zoom) = 0;
    virtual int getPhysicalPosition(qreal *pan, qreal *tilt, qreal *zoom) const = 0;

    virtual int setLogicalPosition(qreal pan, qreal tilt, qreal fov) = 0;
    virtual int getLogicalPosition(qreal *pan, qreal *tilt, qreal *fov) const = 0;
    virtual int getLogicalLimits(QnPtzLimits *limits) = 0;
        
    virtual int updateState() = 0;

protected:
    QnResource* m_resource;
};

#endif // QN_ABSTRACT_PTZ_CONTROLLER_H
