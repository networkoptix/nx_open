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
    QScopedPointer<QnAbstractPtzController> result(new QnVistaMotorPtzController(toSharedPointer(this)));
    if(result->getCapabilities() != Qn::NoPtzCapabilities)
        return result.take(); /* If a camera has motor PTZ, it is not supposed to have any other PTZ. */

    return base_type::createPtzControllerInternal();
}

#endif //ENABLE_ONVIF
