#ifndef QN_DW_ZOOM_PTZ_CONTROLLER_H
#define QN_DW_ZOOM_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <core/ptz/basic_ptz_controller.h>
#include <nx/vms/server/resource/resource_fwd.h>

class QnDwZoomPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnDwZoomPtzController(const QnDigitalWatchdogResourcePtr &resource);
    virtual ~QnDwZoomPtzController();

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;
    virtual bool continuousMove(
        const nx::core::ptz::Vector& speedVector,
        const nx::core::ptz::Options& options) override;

private:
    QnDigitalWatchdogResourcePtr m_resource;
};

#endif // ENABLE_ONVIF

#endif // QN_DW_ZOOM_PTZ_CONTROLLER_H
