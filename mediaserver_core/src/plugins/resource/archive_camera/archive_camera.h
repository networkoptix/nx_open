#ifndef __ARCHIVE_CAMERA_H__
#define __ARCHIVE_CAMERA_H__

#include "core/resource_management/resource_searcher.h"
#include "core/resource/camera_resource.h"

class QnArchiveCamResource 
    : public QnPhysicalCameraResource
{
    Q_OBJECT
public:
    QnArchiveCamResource();

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