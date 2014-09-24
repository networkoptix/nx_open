#ifndef iq_resource_h_1547
#define iq_resource_h_1547

#ifdef ENABLE_IQE

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/media_data_packet.h"

class QnPlIqResource : public QnPhysicalCameraResource
{
public:
    static const QString MANUFACTURE;

    QnPlIqResource();

    virtual bool isResourceAccessible();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames


protected:
    virtual CameraDiagnostics::Result initInternal() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    virtual void setCroppingPhysical(QRect cropping);

    CLHttpStatus readOID(const QString& oid, QString& result);
    CLHttpStatus readOID(const QString& oid, int& result);

    CLHttpStatus setOID(const QString& oid, const QString& val);

    QSize getMaxResolution() const;
    bool isRtp() const;
};

#endif // #ifdef ENABLE_IQE
#endif //iq_resource_h_1547
