/**********************************************************
* 24 apr 2014
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_PTZ_CONTROLLER_H
#define THIRD_PARTY_PTZ_CONTROLLER_H

#ifdef ENABLE_THIRD_PARTY

#include <core/ptz/basic_ptz_controller.h>
#include <plugins/camera_plugin.h>
#include <utils/math/functors.h>

#include "third_party_resource.h"


class QnThirdPartyPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnThirdPartyPtzController(
        const QnThirdPartyResourcePtr& resource,
        nxcip::CameraPtzManager* cameraPtzManager );
    virtual ~QnThirdPartyPtzController();

    virtual Ptz::Capabilities getCapabilities() const override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) const override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) const override;
    virtual bool getFlip(Qt::Orientations *flip) const override;

private:
    QnThirdPartyResourcePtr m_resource;
    nxcip::CameraPtzManager* m_cameraPtzManager;
    Ptz::Capabilities m_capabilities;
    Qt::Orientations m_flip;
    QnPtzLimits m_limits;
    //QnLinearFunction m_35mmEquivToCameraZoom, m_cameraTo35mmEquivZoom;

    //mutable QnMutex m_mutex;
    //QByteArray m_cookie;
};

#endif // ENABLE_THIRD_PARTY

#endif  //THIRD_PARTY_PTZ_CONTROLLER_H
