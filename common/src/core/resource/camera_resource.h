#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include "network_resource.h"
#include "security_cam_resource.h"

class QN_EXPORT QnVirtualCameraResource : virtual public QnNetworkResource, virtual public QnSecurityCamResource
{
    Q_OBJECT

public:
    // TODO: move to QnSecurityCamResource
    enum CameraFlag { CFNoFlags = 0, HasPtz = 1, HasZoom = 2};
    Q_DECLARE_FLAGS(CameraCapabilities, CameraFlag)


    QnVirtualCameraResource();

    virtual void updateInner(QnResourcePtr other) override;

    // TODO: move to QnSecurityCamResource
    void setScheduleDisabled(bool blocked);
    bool isScheduleDisabled() const;

    // TODO: move to QnSecurityCamResource
    bool isAudioEnabled() const;
    void setAudioEnabled(bool value);

    // TODO: move to QnSecurityCamResource
    bool isAdvancedWorking() const;
    void setAdvancedWorking(bool value);

    // TODO: move to QnSecurityCamResource
    CameraCapabilities getCameraCapabilities();
    void addCameraCapabilities(CameraCapabilities value);

signals:
    void scheduleDisabledChanged(const QnVirtualCameraResourcePtr &resource);

private:
    bool m_scheduleDisabled;
    bool m_audioEnabled;
    bool m_advancedWorking;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QnVirtualCameraResource::CameraCapabilities)


class QN_EXPORT QnPhysicalCameraResource : virtual public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnPhysicalCameraResource();

    // returns 0 if primary stream does not exist
    int getPrimaryStreamDesiredFps() const;

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    void onPrimaryFpsUpdated(int newFps);

    virtual int suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const;

#ifdef _DEBUG
    void debugCheck() const;
#endif
};


#endif // QN_CAMERA_RESOURCE_H
