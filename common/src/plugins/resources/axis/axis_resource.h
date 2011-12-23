#ifndef axis_resource_h_2215
#define axis_resource_h_2215


#include "core/resourcemanagment/security_cam_resource.h"
#include "core/resource/network_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"


class QnPlAxisResource : public QnNetworkResource, public QnSequrityCamResource
{
public:

    static const char* MANUFACTURE;

    QnPlAxisResource();

    virtual bool isResourceAccessible();

    virtual bool updateMACAddress();

    virtual QString manufacture() const;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames

protected:

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();

    virtual void setCropingPhysical(QRect croping);
};

#endif //axis_resource_h_2215