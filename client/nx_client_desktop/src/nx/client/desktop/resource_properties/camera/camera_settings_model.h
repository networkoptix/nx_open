#pragma once

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsModel: public Connective<QObject>, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit CameraSettingsModel(QObject* parent = nullptr);
    virtual ~CameraSettingsModel() override;

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList& cameras);

    bool isSingleCameraMode() const;

    QString name() const;
    void setName(const QString& name);

    struct CameraPersistentInfo
    {
        QString id;
        QString model;
        QString vendor;
        QString firmware;
        QString macAddress;
    };
    CameraPersistentInfo info() const;

    struct CameraNetworkInfo
    {
        QString ipAddress;
        QString webPage;
        QString primaryStream;
        QString secondaryStream;
    };
    CameraNetworkInfo networkInfo() const;

signals:
    void networkInfoChanged();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
