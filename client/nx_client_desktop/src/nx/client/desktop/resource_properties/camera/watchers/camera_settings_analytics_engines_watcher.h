#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsAnalyticsEnginesWatcher: public QObject, public QnWorkbenchContextAware
{
    using base_type = QObject;

public:
    CameraSettingsAnalyticsEnginesWatcher(
        CameraSettingsDialogStore* store, QObject* parent = nullptr);
    virtual ~CameraSettingsAnalyticsEnginesWatcher() override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::desktop
