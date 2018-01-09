#pragma once

#ifdef ENABLE_DROID

#include <nx/mediaserver/resource/camera.h>

class QnDroidResource: public nx::mediaserver::resource::Camera
{
    Q_OBJECT

public:
    static const QString MANUFACTURE;

    QnDroidResource();

    virtual int getMaxFps() const override; 
    virtual QString getDriverName() const override;
    virtual void setIframeDistance(int frames, int timems) override; // sets the distance between I frames


    virtual QString getHostAddress() const override;
    virtual void setHostAddress(const QString &ip) override;
protected:
    virtual QnAbstractStreamDataProvider* createLiveDataProvider() override;

private:
};

typedef QnSharedResourcePointer<QnDroidResource> QnDroidResourcePtr;

#endif // #ifdef ENABLE_DROID
