#ifndef pulse_resource_h_1947
#define pulse_resource_h_1947

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

class QnPlPulseResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlPulseResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    

protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //pulse_resource_h_1947
