#pragma once

#ifdef ENABLE_ONVIF

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include <nx/network/deprecated/simple_http_client.h>
#include "nx/streaming/media_data_packet.h"
#include "../onvif/onvif_resource.h"

class onvifXsd__H264Configuration;

class QnFlexWatchResource : public QnPlOnvifResource
{
public:
    QnFlexWatchResource(QnMediaServerModule* serverModule);
    virtual ~QnFlexWatchResource();

protected:
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
private:
    CameraDiagnostics::Result fetchUpdateVideoEncoder();
    bool rebootDevice();
private:
    onvifXsd__H264Configuration* m_tmpH264Conf;
};

typedef QnSharedResourcePointer<QnFlexWatchResource> QnFlexWatchResourcePtr;

#endif //ENABLE_ONVIF
