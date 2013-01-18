#ifndef QN_CAMERA_RESOURCE_H
#define QN_CAMERA_RESOURCE_H

#include <QtCore/QMetaType>

#include "network_resource.h"
#include "security_cam_resource.h"

class QnAbstractDTSFactory;

class QN_EXPORT QnVirtualCameraResource : virtual public QnNetworkResource, virtual public QnSecurityCamResource
{
    Q_OBJECT

public:
    QnVirtualCameraResource();

    virtual void updateInner(QnResourcePtr other) override;

    // TODO: move to QnSecurityCamResource
    void setScheduleDisabled(bool blocked);
    bool isScheduleDisabled() const;

    // TODO: move to QnSecurityCamResource
    bool isAudioEnabled() const;
    void setAudioEnabled(bool value);

    bool isManuallyAdded() const;
    void setManuallyAdded(bool value);

    // TODO: move to QnSecurityCamResource
    bool isAdvancedWorking() const;
    void setAdvancedWorking(bool value);

    QnAbstractDTSFactory* getDTSFactory();
    void setDTSFactory(QnAbstractDTSFactory* factory);
    void lockDTSFactory();
    void unLockDTSFactory();

    // TODO: move to QnSecurityCamResource
    QString getModel() const;
    void setModel(QString model);

    // TODO: move to QnSecurityCamResource
    QString getFirmware() const;
    void setFirmware(QString firmware);

signals:
    void scheduleDisabledChanged(const QnVirtualCameraResourcePtr &resource);
    virtual void scheduleTasksChanged(const QnSecurityCamResourcePtr &resource) override;

private:
    bool m_scheduleDisabled;
    bool m_audioEnabled;
    bool m_manuallyAdded;
    bool m_advancedWorking;

    QString m_model;
    QString m_firmware;

    QnAbstractDTSFactory* m_dtsFactory;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QnVirtualCameraResource::CameraCapabilities)


class QN_EXPORT QnPhysicalCameraResource : virtual public QnVirtualCameraResource
{
    Q_OBJECT
public:
    QnPhysicalCameraResource();

    // the difference between desired and real is that camera can have multiple clients we do not know about or big exposure time
    int getPrimaryStreamRealFps() const;

    virtual int suggestBitrateKbps(QnStreamQuality q, QSize resolution, int fps) const;

};

Q_DECLARE_METATYPE(QnVirtualCameraResourcePtr);
Q_DECLARE_METATYPE(QnVirtualCameraResourceList);

#endif // QN_CAMERA_RESOURCE_H
