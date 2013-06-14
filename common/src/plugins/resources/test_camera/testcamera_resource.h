#ifndef __TEST_CAMERA_RESOURCE_H__
#define __TEST_CAMERA_RESOURCE_H__

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"


class QnTestCameraResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnTestCameraResource();

    virtual int getMaxFps() override; 
    virtual bool isResourceAccessible() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QString getHostAddress() const override;
    virtual bool setHostAddress(const QString &ip, QnDomain domain) override;
    virtual bool shoudResolveConflicts() const override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping) override;

private:
};

typedef QnSharedResourcePointer<QnTestCameraResource> QnTestCameraResourcePtr;

#endif //__TEST_CAMERA_RESOURCE_H__
