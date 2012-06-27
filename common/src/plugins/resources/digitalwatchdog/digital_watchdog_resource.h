#ifndef isd_resource_h_1934
#define isd_resource_h_1934

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"
#include "../onvif/onvif_resource.h"

class QnPlWatchDogResource : public QnPlOnvifResource
{
public:
    QnPlWatchDogResource();

    virtual int suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const override;
protected:
    bool initInternal() override;
private:
    bool isDualStreamingEnabled();
};

typedef QnSharedResourcePointer<QnPlWatchDogResource> QnPlWatchDogResourcePtr;

#endif //isd_resource_h_1934
