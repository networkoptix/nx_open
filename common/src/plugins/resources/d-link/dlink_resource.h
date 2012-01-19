#ifndef dlink_resource_h_2215
#define dlink_resource_h_2215

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"

class QnPlDlinkResource : public QnCameraResource
{
public:
    static const char* MANUFACTURE;

    QnPlDlinkResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //dlink_resource_h_2215
