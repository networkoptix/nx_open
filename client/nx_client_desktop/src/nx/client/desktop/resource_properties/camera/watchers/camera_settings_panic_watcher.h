#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsDialogStore;

class CameraSettingsPanicWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsPanicWatcher(QObject* parent = nullptr);

    void setStore(CameraSettingsDialogStore* store);
};

} // namespace desktop
} // namespace client
} // namespace nx
