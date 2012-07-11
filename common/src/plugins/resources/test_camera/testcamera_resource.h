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
    virtual bool updateMACAddress() override;
    virtual QString manufacture() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QHostAddress getHostAddress() const override;
    virtual bool setHostAddress(const QHostAddress &ip, QnDomain domain) override;
    virtual bool shoudResolveConflicts() const override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping) override;

private:
};

typedef QnSharedResourcePointer<QnTestCameraResource> QnTestCameraResourcePtr;

#endif //__TEST_CAMERA_RESOURCE_H__
