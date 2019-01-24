#pragma once

#include <nx/vms/server/resource/camera.h>
#include <nx/network/deprecated/simple_http_client.h>
#include "nx/streaming/media_data_packet.h"

class QnPlIqResource: public nx::vms::server::resource::Camera
{
public:
    static const QString MANUFACTURE;

    QnPlIqResource(QnMediaServerModule* serverModule);

    virtual QString getDriverName() const override;

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDriver(
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
