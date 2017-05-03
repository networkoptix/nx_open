#ifndef QN_MAPPED_PTZ_CONTROLLER_H
#define QN_MAPPED_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"
#include "ptz_mapper.h"

/**
 * A proxy ptz controller that uses a PTZ space mapper to provide absolute
 * PTZ commands in standard PTZ space.
 */
class QnMappedPtzController: public QnProxyPtzController {
    Q_OBJECT;
    typedef QnProxyPtzController base_type;

public:
    QnMappedPtzController(const QnPtzMapperPtr &mapper, const QnPtzControllerPtr &baseController);

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities() const override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const override;

private:
    QnPtzMapperPtr m_mapper;
    QnPtzLimits m_limits;
};


#endif // QN_MAPPED_PTZ_CONTROLLER_H
