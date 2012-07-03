#ifndef sony_resource_h_1855
#define sony_resource_h_1855

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"
#include "../onvif/onvif_resource.h"

class QnPlSonyResource : public QnPlOnvifResource
{
public:
    QnPlSonyResource();
protected:
    virtual void updateResourceCapabilities() override;
private:

};

typedef QnSharedResourcePointer<QnPlSonyResource> QnPlSonyResourcePtr;

#endif //sony_resource_h_1855
