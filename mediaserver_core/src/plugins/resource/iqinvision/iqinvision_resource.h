#pragma once

#ifdef ENABLE_IQE

#include <nx/mediaserver/resource/camera.h>
#include <nx/network/deprecated/simple_http_client.h>
#include "nx/streaming/media_data_packet.h"

class QnPlIqResource : public nx::mediaserver::resource::Camera
{
public:
    static const QString MANUFACTURE;

    QnPlIqResource();

    virtual QString getDriverName() const override;

    virtual void setIframeDistance(int frames, int timems); // sets the distance between I frames


protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;

    virtual QnAbstractStreamDataProvider* createLiveDataProvider();
    virtual void setCroppingPhysical(QRect cropping);

    CLHttpStatus readOID(const QString& oid, QString& result);
    CLHttpStatus readOID(const QString& oid, int& result);

    CLHttpStatus setOID(const QString& oid, const QString& val);

    QSize getMaxResolution() const;
    bool isRtp() const;
};

#endif // #ifdef ENABLE_IQE
