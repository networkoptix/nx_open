#pragma once

#include <common/common_globals.h>
#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>

namespace nx::vms::client::core {

class ResourceHelper: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId NOTIFY resourceIdChanged)
    Q_PROPERTY(Qn::ResourceStatus resourceStatus READ resourceStatus NOTIFY resourceStatusChanged)
    Q_PROPERTY(QString resourceName READ resourceName NOTIFY resourceNameChanged)
    Q_PROPERTY(qint64 serverTimeOffset READ serverTimeOffset NOTIFY serverTimeOffsetChanged)
    Q_PROPERTY(bool hasDefaultCameraPassword READ hasDefaultCameraPassword
        NOTIFY defaultCameraPasswordChanged)
    Q_PROPERTY(bool hasOldCameraFirmware READ hasOldCameraFirmware
        NOTIFY oldCameraFirmwareChanged)

    Q_PROPERTY(bool audioSupported READ audioSupported NOTIFY audioSupportedChanged)
    Q_PROPERTY(bool isIoModule READ isIoModule NOTIFY isIoModuleChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)

    Q_ENUMS(Qn::ResourceStatus)

    using base_type = Connective<QObject>;

public:
    ResourceHelper(QObject* parent = nullptr);

    QString resourceId() const;
    void setResourceId(const QString& id);

    Qn::ResourceStatus resourceStatus() const;
    QString resourceName() const;
    bool hasDefaultCameraPassword() const;
    bool hasOldCameraFirmware() const;

    bool audioSupported() const;
    bool isIoModule() const;
    bool hasVideo() const;

    qint64 serverTimeOffset() const;

signals:
    void resourceIdChanged();
    void resourceStatusChanged();
    void resourceNameChanged();
    void defaultCameraPasswordChanged();
    void oldCameraFirmwareChanged();
    void serverTimeOffsetChanged();
    void audioSupportedChanged();
    void isIoModuleChanged();
    void hasVideoChanged();

private:
    bool hasCameraCapability(Qn::CameraCapability capability) const;

protected:
    QnResourcePtr resource() const;

private:
    QnResourcePtr m_resource;
};

} // namespace nx::vms::client::core
