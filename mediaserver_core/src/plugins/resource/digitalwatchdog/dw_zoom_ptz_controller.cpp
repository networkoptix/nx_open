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

Ptz::Capabilities QnDwZoomPtzController::getCapabilities() const
{
    return Ptz::ContinuousZoomCapability;
}

bool QnDwZoomPtzController::continuousMove(const QVector3D &speed) {
    const QString query = lit("/cgi-bin/ptzctrl.cgi?ptzchannel=0&query=zoom&ptzctrlvalue=%1");

    QString value;

    if(qFuzzyIsNull(speed.z())) {
        value = lit("stop");
    } else if(speed.z() < 0.0) {
        value = lit("zoomOut");
    } else if(speed.z() > 0.0) {
        value = lit("zoomIn");
    }

    return m_resource->httpClient().doGET(query.arg(value)) == CL_HTTP_SUCCESS;
}

#endif //ENABLE_ONVIF
