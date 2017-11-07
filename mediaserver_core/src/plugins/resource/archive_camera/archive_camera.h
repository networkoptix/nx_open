#ifndef __ARCHIVE_CAMERA_H__
#define __ARCHIVE_CAMERA_H__

#include "core/resource_management/resource_searcher.h"
#include "core/resource/camera_resource.h"

class QnArchiveCamResourceSearcher : public QnAbstractNetworkResourceSearcher
{
    using base_type = QnAbstractNetworkResourceSearcher;
public:
    QnArchiveCamResourceSearcher(QnCommonModule* commonModule);

    virtual void pleaseStop() override;

    bool isProxy() const;

    virtual QnResourceList findResources() override;

    virtual QnResourcePtr createResource(const QnUuid &resourceTypeId, const QnResourceParams& params) override;

    virtual QString manufacture() const override;

    virtual QList<QnResourcePtr> checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool doMultichannelCheck) override;
};

class QnArchiveCamResource
    : public QnPhysicalCameraResource
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
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};
#endif
