#ifdef ENABLE_AXIS
#ifdef ENABLE_ONVIF

#include "axis_onvif_resource.h"

QnAxisOnvifResource::QnAxisOnvifResource()
{
}

int QnAxisOnvifResource::suggestBitrateForQualityKbps(Qn::StreamQuality q, QSize resolution, int fps, Qn::ConnectionRole role) const
{
    Q_UNUSED(q)
    Q_UNUSED(resolution)
    Q_UNUSED(fps)
    Q_UNUSED(role)
    return 0;
}

#endif  //ENABLE_ONVIF
#endif // #ifdef ENABLE_AXIS