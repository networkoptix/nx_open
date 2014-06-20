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

int QnVistaResource::suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const {
    int result = QnPlOnvifResource::suggestBitrateKbps(quality, resolution, fps);

    return result;
}

QnAbstractPtzController *QnVistaResource::createPtzControllerInternal() {
    QScopedPointer<QnAbstractPtzController> result(base_type::createPtzControllerInternal());
    if(!result)
        return NULL;

    return new QnVistaFocusPtzController(QnPtzControllerPtr(result.take()));
}

bool QnVistaResource::startInputPortMonitoring()
{
    if( hasFlags(QnResource::foreigner) )     //we do not own camera
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
