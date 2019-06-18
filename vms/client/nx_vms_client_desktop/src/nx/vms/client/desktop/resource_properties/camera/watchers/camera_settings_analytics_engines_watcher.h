#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsAnalyticsEnginesWatcher: public QObject, public QnWorkbenchContextAware
{
    using base_type = QObject;

public:
    CameraSettingsAnalyticsEnginesWatcher(
        CameraSettingsDialogStore* store, QObject* parent = nullptr);
    virtual ~CameraSettingsAnalyticsEnginesWatcher() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
