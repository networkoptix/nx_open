#pragma once

#include <core/resource_management/resource_searcher.h>
#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/server_module_aware.h>

class QnArchiveCamResourceSearcher:
    public QnAbstractNetworkResourceSearcher,
    public nx::vms::server::ServerModuleAware
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnArchiveCamResourceSearcher(QnMediaServerModule* serverModule);

    virtual void pleaseStop() override;

    bool isProxy() const;

    virtual QnResourceList findResources() override;

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId,
        const QnResourceParams& params) override;

    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url,
        const QAuthenticator& auth, bool doMultichannelCheck) override;
};

class QnArchiveCamResource:
    public nx::vms::server::resource::Camera
{
    Q_OBJECT
public:
    QnArchiveCamResource(QnMediaServerModule* serverModule, const QnResourceParams& params);

public:
    virtual void checkIfOnlineAsync(std::function<void(bool)> completionHandler) override;

    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems);
    virtual void setMotionMaskPhysical(int channel) override;

    static QString cameraName();

protected:
    virtual nx::vms::server::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(
        Qn::StreamIndex streamIndex) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};
