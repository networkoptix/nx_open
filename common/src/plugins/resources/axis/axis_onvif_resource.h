#ifndef __AXIS_ONVIF_RESOURCE_H__
#define __AXIS_ONVIF_RESOURCE_H__

#include <map>

#include <QMutex>

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
    static int MAX_RESOLUTION_DECREASES_NUM;

    Q_OBJECT

public:
    QnAxisOnvifResource();
    virtual int suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const override;
};

typedef QnSharedResourcePointer<QnAxisOnvifResource> QnAxisOnvifResourcePtr;

#endif //__AXIS_ONVIF_RESOURCE_H__
