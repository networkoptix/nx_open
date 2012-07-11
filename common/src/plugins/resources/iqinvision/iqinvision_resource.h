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


protected:
    bool initInternal() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    virtual void setCropingPhysical(QRect croping);

    CLHttpStatus readOID(const QString& oid, QString& result);
    CLHttpStatus readOID(const QString& oid, int& result);

    CLHttpStatus setOID(const QString& oid, const QString& val);

    QSize getMaxResolution() const;
};

#endif //iq_resource_h_1547
