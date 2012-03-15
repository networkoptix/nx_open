#ifndef droid_resource_h_18_04
#define droid_resource_h_18_04

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"


class QnDroidResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const char* MANUFACTURE;

    QnDroidResource();

    virtual int getMaxFps() override; 
    virtual bool isResourceAccessible() override;
    virtual bool updateMACAddress() override;
    virtual QString manufacture() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames

    virtual bool hasDualStreaming() const override;

    virtual QHostAddress getHostAddress() const override;
    virtual bool setHostAddress(const QHostAddress &ip, QnDomain domain) override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping) override;

private:
};

typedef QnSharedResourcePointer<QnDroidResource> QnDroidResourcePtr;

#endif //droid_resource_h_18_04