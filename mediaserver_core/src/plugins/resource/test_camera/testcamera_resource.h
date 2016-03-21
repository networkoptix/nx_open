#ifndef __TEST_CAMERA_RESOURCE_H__
#define __TEST_CAMERA_RESOURCE_H__

#ifdef ENABLE_TEST_CAMERA

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"


class QnTestCameraResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnTestCameraResource();

    virtual int getMaxFps() const override; 
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QString getHostAddress() const override;
    virtual void setHostAddress(const QString &ip) override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

private:
};

typedef QnSharedResourcePointer<QnTestCameraResource> QnTestCameraResourcePtr;

#endif // #ifdef ENABLE_TEST_CAMERA
#endif //__TEST_CAMERA_RESOURCE_H__
