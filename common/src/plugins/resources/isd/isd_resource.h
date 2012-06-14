#ifndef isd_resource_h_1934
#define isd_resource_h_1934

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class QnPlIsdResource : public QnPhysicalCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlIsdResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

    bool hasDualStreaming() const override {return false;}

    virtual const QnResourceAudioLayout* getAudioLayout(const QnAbstractMediaStreamDataProvider* dataProvider) override;
protected:

    void initInternal() override {}

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //isd_resource_h_1934
