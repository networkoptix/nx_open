#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {

class AbstractTextProvider;
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

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList& value);

    AbstractTextProvider* licenseUsageTextProvider() const;

private:
    class Private;
    Private* const d = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
