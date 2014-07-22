#ifndef droid_resource_h_18_04
#define droid_resource_h_18_04

#ifdef ENABLE_DROID

#include "core/resource/security_cam_resource.h"
#include "core/resource/camera_resource.h"


class QnDroidResource : public QnPhysicalCameraResource
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnDroidResource();

    virtual int getMaxFps() const override; 
    virtual bool isResourceAccessible() override;
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QString getHostAddress() const override;
    virtual bool setHostAddress(const QString &ip, QnDomain domain) override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

private:
};

typedef QnSharedResourcePointer<QnDroidResource> QnDroidResourcePtr;

#endif // #ifdef ENABLE_DROID
#endif //droid_resource_h_18_04
