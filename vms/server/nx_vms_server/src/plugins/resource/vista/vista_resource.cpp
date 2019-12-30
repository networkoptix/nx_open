#ifdef ENABLE_ONVIF

#include "vista_resource.h"
#include "vista_focus_ptz_controller.h"
#include "soap/soapserver.h"

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

#endif //ENABLE_ONVIF
