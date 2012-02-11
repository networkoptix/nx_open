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

    void setDataPort(int port);
    int getDataPort() const;

    void setVideoPort(int port);
    int getVideoPort() const;


protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;
    virtual void setCropingPhysical(QRect croping) override;

private:
    int m_videoPort;
    int m_dataPort;
};

typedef QSharedPointer<QnDroidResource> QnDroidResourcePtr;

#endif //droid_resource_h_18_04