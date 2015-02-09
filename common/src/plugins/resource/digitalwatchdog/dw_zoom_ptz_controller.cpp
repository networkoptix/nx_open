#ifdef ENABLE_ONVIF

#include "dw_zoom_ptz_controller.h"

#include "digital_watchdog_resource.h"

QnDwZoomPtzController::QnDwZoomPtzController(const QnDigitalWatchdogResourcePtr &resource):
    base_type(resource),
    m_resource(resource)
{}

QnDwZoomPtzController::~QnDwZoomPtzController() {
    return;
}

Qn::PtzCapabilities QnDwZoomPtzController::getCapabilities() {
    return Qn::ContinuousZoomCapability;
}

bool QnDwZoomPtzController::continuousMove(const QVector3D &speed) {
    const QString paramId = lit("/cgi-bin/ptzctrl.cgi?ptzchannel=0&query=zoom&ptzctrlvalue");
    QString value;

    if(qFuzzyIsNull(speed.z())) {
        value = lit("stop");
    } else if(speed.z() < 0.0) {
        value = lit("zoomOut");
    } else if(speed.z() > 0.0) {
        value = lit("zoomIn");
    }

    m_resource->setParamPhysical(paramId, value);

    return true;
}

#endif //ENABLE_ONVIF
