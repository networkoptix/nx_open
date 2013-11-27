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

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual int absoluteMove(const QVector3D &position) override;
    virtual int getPosition(QVector3D *position) override;
    virtual int getLimits(QnPtzLimits *limits) override;

private:
    QnPtzControllerPtr m_controller;
    QnPtzMapperPtr m_mapper;
    QnPtzLimits m_limits;
};


#endif // QN_MAPPED_PTZ_CONTROLLER_H
