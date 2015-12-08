#ifndef __ARCHIVE_CAMERA_H__
#define __ARCHIVE_CAMERA_H__

#include "core/resource_management/resource_searcher.h"
#include "core/resource/camera_resource.h"

class QnArchiveCamResourceSearcher 
    : public QnAbstractNetworkResourceSearcher 
{
public:
    virtual QnResourcePtr createResource(
        const QnUuid            &resourceTypeId, 
        const QnResourceParams  &params 
    ) override;

    virtual QList<QnResourcePtr> checkHostAddr(
        const QUrl              &url, 
        const QAuthenticator    &auth, 
        bool                    doMultichannelCheck
    ) override;
protected:
    virtual QString manufacture() const;
};


class QnArchiveCamResource 
    : public QnPhysicalCameraResource
{
    Q_OBJECT
public:
    QnArchiveCamResource();

public:
    virtual bool checkIfOnlineAsync(
        std::function<void(bool)>&& completionHandler
    ) override;

    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems); 
    virtual void setMotionMaskPhysical(int channel) override;

protected:
    virtual CameraDiagnostics::Result initInternal() override;
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
};
#endif