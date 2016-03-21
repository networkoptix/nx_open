#ifndef __AXIS_ONVIF_RESOURCE_H__
#define __AXIS_ONVIF_RESOURCE_H__

#ifdef ENABLE_AXIS
#ifdef ENABLE_ONVIF

#include <plugins/resource/onvif/onvif_resource.h>

class QnAxisOnvifResource : public QnPlOnvifResource
{
    Q_OBJECT
    static int MAX_RESOLUTION_DECREASES_NUM;
public:
    QnAxisOnvifResource();
    virtual int suggestBitrateKbps(Qn::StreamQuality q, QSize resolution, int fps) const override;
};

typedef QnSharedResourcePointer<QnAxisOnvifResource> QnAxisOnvifResourcePtr;

#endif  //ENABLE_ONVIF
#endif // ENABLE_AXIS
#endif //__AXIS_ONVIF_RESOURCE_H__
