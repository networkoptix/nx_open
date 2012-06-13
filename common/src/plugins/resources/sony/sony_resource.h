#ifndef sony_resource_h_329
#define sony_resource_h_329

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class QnPlSonyResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlSonyResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool hasDualStreaming() const override {return false;}

protected:

    void initInternal() override {}

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //sony_resource_h_329
