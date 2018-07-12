/**********************************************************
* 24 apr 2014
* akolesnikov
***********************************************************/

#ifndef THIRD_PARTY_PTZ_CONTROLLER_H
#define THIRD_PARTY_PTZ_CONTROLLER_H

#ifdef ENABLE_THIRD_PARTY

#include <core/ptz/basic_ptz_controller.h>
#include <camera/camera_plugin.h>
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

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speedVector,
        const nx::core::ptz::Options& options) override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* outPosition,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space, QnPtzLimits* limits,
        const nx::core::ptz::Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations* flip,
        const nx::core::ptz::Options& options) const override;

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
