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

QnAbstractPtzController *QnVistaResource::createPtzControllerInternal() {
    QScopedPointer<QnAbstractPtzController> result(base_type::createPtzControllerInternal());
    if(!result)
        return NULL;

    return new QnVistaFocusPtzController(QnPtzControllerPtr(result.take()));
}

//bool QnVistaResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ )
//{
//    if( hasFlags(Qn::foreigner) ||      //we do not own camera
//        !hasCameraCapabilities(Qn::RelayInputCapability) )
//    {
//        return false;
//    }
//
//    if( !m_eventCapabilities.get() )
//        return false;
//
//    //although Vista reports that it supports PullPoint subscription, it does not work...
//    if( QnSoapServer::instance()->initialized() )
//        return registerNotificationConsumer();
//    return false;
//}

#endif //ENABLE_ONVIF
