#ifndef __FLEXWATCH_RESOURCE_H__
#define __FLEXWATCH_RESOURCE_H__

#ifdef ENABLE_ONVIF

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"
#include "../onvif/onvif_resource.h"

class onvifXsd__H264Configuration;

class QnFlexWatchResource : public QnPlOnvifResource
{
public:
    QnFlexWatchResource();
    virtual ~QnFlexWatchResource();

protected:
    virtual CameraDiagnostics::Result initInternal() override;
private:
    CameraDiagnostics::Result fetchUpdateVideoEncoder();
    bool rebootDevice();
private:
    onvifXsd__H264Configuration* m_tmpH264Conf;
};

typedef QnSharedResourcePointer<QnFlexWatchResource> QnFlexWatchResourcePtr;

#endif //ENABLE_ONVIF

#endif // __FLEXWATCH_RESOURCE_H__
