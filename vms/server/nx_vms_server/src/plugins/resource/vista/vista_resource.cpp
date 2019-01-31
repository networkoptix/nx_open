#ifdef ENABLE_ONVIF

#include "vista_resource.h"
#include "vista_focus_ptz_controller.h"
#include "soap/soapserver.h"

// Although Vista reports that it supports PullPoint subscription, it does not work...
static const bool isIoSupported = false;

QnVistaResource::QnVistaResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule)
{
    setVendor(lit("VISTA"));
}

QnVistaResource::~QnVistaResource() {
    return;
}

QnAbstractPtzController* QnVistaResource::createPtzControllerInternal() const
{
    QScopedPointer<QnAbstractPtzController> result(base_type::createPtzControllerInternal());
    if(!result)
        return NULL;

    return new QnVistaFocusPtzController(QnPtzControllerPtr(result.take()));
}

void QnVistaResource::startInputPortStatesMonitoring()
{
    if (!isIoSupported)
        return;

    if (!m_eventCapabilities.get())
        return;

    if (QnSoapServer::instance()->initialized())
        registerNotificationConsumer();
}

#endif //ENABLE_ONVIF
