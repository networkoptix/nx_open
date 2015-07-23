#ifndef QN_DW_ZOOM_PTZ_CONTROLLER_H
#define QN_DW_ZOOM_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF
#include <core/ptz/basic_ptz_controller.h>

class QnDwZoomPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnDwZoomPtzController(const QnDigitalWatchdogResourcePtr &resource);
    virtual ~QnDwZoomPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;

private:
    QnDigitalWatchdogResourcePtr m_resource;
};

#endif // ENABLE_ONVIF

#endif // QN_DW_ZOOM_PTZ_CONTROLLER_H
