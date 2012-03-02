#ifndef iq_resource_h_1547
#define iq_resource_h_1547

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class QnPlIqResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlIqResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool hasDualStreaming() const override {return false;}

protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //iq_resource_h_1547
