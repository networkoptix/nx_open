#ifndef sony_resource_h_1855
#define sony_resource_h_1855

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"
#include "../onvif/onvif_resource.h"

class QnPlSonyResource : public QnPlOnvifResource
{
    static int MAX_RESOLUTION_DECREASES_NUM;

public:
    QnPlSonyResource();

    virtual int getMaxOnvifRequestTries() const override { return 5; };

protected:
    virtual bool updateResourceCapabilities() override;
private:

};

typedef QnSharedResourcePointer<QnPlSonyResource> QnPlSonyResourcePtr;

#endif //sony_resource_h_1855
