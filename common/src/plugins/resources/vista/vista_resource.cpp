#ifdef ENABLE_ONVIF

#include "vista_resource.h"
#include "vista_motor_ptz_controller.h"

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

#endif //ENABLE_ONVIF
