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

bool QnDwZoomPtzController::continuousMove(const nx::core::ptz::Vector& speedVector)
{
    const QString query = lit("/cgi-bin/ptzctrl.cgi?ptzchannel=0&query=zoom&ptzctrlvalue=%1");

    QString value;

    if(qFuzzyIsNull(speedVector.zoom)) {
        value = lit("stop");
    } else if(speedVector.zoom < 0.0) {
        value = lit("zoomOut");
    } else if(speedVector.zoom > 0.0) {
        value = lit("zoomIn");
    }

    return m_resource->httpClient().doGET(query.arg(value)) == CL_HTTP_SUCCESS;
}

#endif //ENABLE_ONVIF
