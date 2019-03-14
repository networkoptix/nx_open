#pragma once

#ifdef ENABLE_AXIS
#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

class QnAxisOnvifResource : public QnPlOnvifResource
{
    Q_OBJECT
    static int MAX_RESOLUTION_DECREASES_NUM;
public:
    QnAxisOnvifResource(QnMediaServerModule* serverModule);
    virtual int suggestBitrateForQualityKbps(Qn::StreamQuality q, QSize resolution, int fps,
        const QString& codec, Qn::ConnectionRole role = Qn::CR_Default) const override;
};

typedef QnSharedResourcePointer<QnAxisOnvifResource> QnAxisOnvifResourcePtr;

#endif  //ENABLE_ONVIF
#endif // ENABLE_AXIS
