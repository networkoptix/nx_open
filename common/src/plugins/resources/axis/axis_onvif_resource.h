#ifndef __AXIS_ONVIF_RESOURCE_H__
#define __AXIS_ONVIF_RESOURCE_H__

#ifndef DISABLE_ONVIF

#include <map>

#include <QtCore/QMutex>

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/http/linesplitter.h"
#include "core/datapacket/media_data_packet.h"
#include "../onvif/onvif_resource.h"


namespace nx_http
{
    class AsyncHttpClient;
}

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

#endif //__AXIS_ONVIF_RESOURCE_H__
