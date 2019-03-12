#ifdef ENABLE_AXIS
#ifdef ENABLE_ONVIF

#include "axis_onvif_resource.h"

QnAxisOnvifResource::QnAxisOnvifResource(QnMediaServerModule* serverModule):
    QnPlOnvifResource(serverModule)
{
}

int QnAxisOnvifResource::suggestBitrateForQualityKbps(Qn::StreamQuality, QSize /*resolution*/,
    int /*fps*/, const QString& /*codec*/, Qn::ConnectionRole) const
{
    return 0;
}

#endif  //ENABLE_ONVIF
#endif // #ifdef ENABLE_AXIS