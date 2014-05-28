#ifdef ENABLE_ONVIF

#include "vista_resource.h"


QnVistaResource::QnVistaResource():
    QnPlOnvifResource()
{
    setVendor(lit("VISTA"));
}

QnVistaResource::~QnVistaResource()
{
}

int QnVistaResource::suggestBitrateKbps(Qn::StreamQuality quality, QSize resolution, int fps) const
{
    int result = QnPlOnvifResource::suggestBitrateKbps(quality, resolution, fps);

    return result;
}

#endif //ENABLE_ONVIF
