#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsModel: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit CameraSettingsModel(QnWorkbenchContext* context, QObject* parent = nullptr);
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

    void pingCamera();
    void showCamerasOnLayout();
    void openEventLog();
    void openCameraRules();

signals:
    void networkInfoChanged();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
