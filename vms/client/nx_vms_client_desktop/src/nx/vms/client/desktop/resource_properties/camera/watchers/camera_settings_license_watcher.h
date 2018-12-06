#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class LicenseUsageProvider;
class CameraSettingsDialogStore;

class CameraSettingsLicenseWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsLicenseWatcher(CameraSettingsDialogStore* store,
        QObject* parent = nullptr);

    virtual ~CameraSettingsLicenseWatcher() override;

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList& value);

    LicenseUsageProvider* licenseUsageProvider() const;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
