#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include "network_resource.h"
#include "security_cam_resource.h"

class QN_EXPORT QnVirtualCameraResource : virtual public QnNetworkResource, virtual public QnSecurityCamResource
{
    Q_OBJECT

public:
    QnVirtualCameraResource();

    virtual void updateInner(QnResourcePtr other) override;

    void setScheduleDisabled(bool blocked);
    bool isScheduleDisabled() const;

    void setAudioEnabled(bool enabled);
    bool isAudioEnabled() const;
private:
    bool m_scheduleDisabled;
    bool m_audioEnabled;
};


class QN_EXPORT QnPhysicalCameraResource : virtual public QnVirtualCameraResource
{
    Q_OBJECT
public:
    // returns 0 if primary stream does not exist
    int getPrimaryStreamDesiredFps() const;

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    void onPrimaryFpsUpdated(int newFps);

#ifdef _DEBUG
    void debugCheck() const;
#endif
};


#endif // QN_CAMERA_RESOURCE_H
