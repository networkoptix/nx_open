#pragma once

#include <core/resource_management/resource_searcher.h>
#include <nx/mediaserver/resource/camera.h>

class QnArchiveCamResourceSearcher:
    public QnAbstractNetworkResourceSearcher
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnArchiveCamResourceSearcher(QnCommonModule* commonModule);

    virtual void pleaseStop() override;

    bool isProxy() const;

    virtual QnResourceList findResources() override;

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
};

class QnArchiveCamResource:
    public nx::mediaserver::resource::Camera
{
    Q_OBJECT
public:
    QnArchiveCamResource(const QnResourceParams &params);

public:
    virtual void checkIfOnlineAsync(std::function<void(bool)> completionHandler) override;

    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems);
    virtual void setMotionMaskPhysical(int channel) override;

    static QString cameraName();

protected:
    virtual nx::mediaserver::resource::StreamCapabilityMap getStreamCapabilityMapFromDrives(bool primaryStream) override;
    virtual CameraDiagnostics::Result initializeCameraDriver() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};
