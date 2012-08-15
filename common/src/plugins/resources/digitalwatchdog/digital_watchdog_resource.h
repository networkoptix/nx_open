#ifndef dw_resource_h_1854
#define dw_resource_h_1854

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"
#include "utils/network/simple_http_client.h"
#include "core/datapacket/mediadatapacket.h"
#include "../onvif/onvif_resource.h"

class QnPlWatchDogResource : public QnPlOnvifResource
{
public:
    QnPlWatchDogResource();

    virtual int suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const override;

protected:
    bool initInternal() override;
    virtual void fetchAndSetCameraSettings() override;
    virtual bool getParamPhysical(const QnParam &param, QVariant &val) override;
    virtual bool setParamPhysical(const QnParam &param, const QVariant& val) override;

private:
    bool isDualStreamingEnabled();
};

typedef QnSharedResourcePointer<QnPlWatchDogResource> QnPlWatchDogResourcePtr;

#endif //dw_resource_h_1854
