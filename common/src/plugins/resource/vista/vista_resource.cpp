#ifdef ENABLE_ONVIF

#include "vista_resource.h"
#include "vista_focus_ptz_controller.h"
#include "soap/soapserver.h"


QnVistaResource::QnVistaResource() {
    setVendor(lit("VISTA"));
}

QnVistaResource::~QnVistaResource() {
    return;
}

int QnVistaResource::suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const 
{
    // I assume for a Qn::QualityHighest quality 30 fps for 1080 we need 10 mbps
    // I assume for a Qn::QualityLowest quality 30 fps for 1080 we need 1 mbps

    int hiEnd = 1024*6;
    int lowEnd = 1024*1;

    float resolutionFactor = resolution.width()*resolution.height()/1920.0/1080;
    resolutionFactor = pow(resolutionFactor, (float)0.5);

    float frameRateFactor = fps/30.0;
    frameRateFactor = pow(frameRateFactor, (float)0.4);

    int result = lowEnd + (hiEnd - lowEnd) * (quality - Qn::QualityLowest) / (Qn::QualityHighest - Qn::QualityLowest);
    result *= (resolutionFactor * frameRateFactor);

    return qMax(192,result);
}

QnAbstractPtzController *QnVistaResource::createPtzControllerInternal() {
    QScopedPointer<QnAbstractPtzController> result(base_type::createPtzControllerInternal());
    if(!result)
        return NULL;

    return new QnVistaFocusPtzController(QnPtzControllerPtr(result.take()));
}

bool QnVistaResource::startInputPortMonitoring()
{
    if( hasFlags(Qn::foreigner) )     //we do not own camera
    {
        return false;
    }

    if( !m_eventCapabilities.get() )
        return false;

    //although Vista reports that it supports PullPoint subscription, it does not work...
    if( QnSoapServer::instance()->initialized() )
        return registerNotificationConsumer();
    return false;
}

#endif //ENABLE_ONVIF
