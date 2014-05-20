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

int QnVistaResource::suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const
{
    int result = QnPlOnvifResource::suggestBitrateKbps(q, resolution, fps);

    return result;
}

#endif //ENABLE_ONVIF
