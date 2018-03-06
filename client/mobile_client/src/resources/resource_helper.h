#pragma once

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>

class QnResourceHelper: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(Qn::ResourceStatus resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(bool hasDefaultCameraPassword READ hasDefaultCameraPassword
        NOTIFY defaultCameraPasswordChanged)
    Q_PROPERTY(bool hasOldCameraFirmware READ hasOldCameraFirmware
        NOTIFY oldCameraFirmwareChanged)
    Q_ENUMS(Qn::ResourceStatus)

    using base_type = Connective<QObject>;

public:
    QnResourceHelper(QObject* parent = nullptr);

    QString resourceId() const;
    void setResourceId(const QString& id);

    Qn::ResourceStatus resourceStatus() const;
    QString resourceName() const;
    bool hasDefaultCameraPassword() const;
    bool hasOldCameraFirmware() const;

signals:
    void resourceIdChanged();
    void resourceStatusChanged();
    void resourceNameChanged();
    void defaultCameraPasswordChanged();
    void oldCameraFirmwareChanged();

private:
    bool hasCameraCapability(Qn::CameraCapability capability) const;

protected:
    QnResourcePtr resource() const;

private:
    QnResourcePtr m_resource;
};
