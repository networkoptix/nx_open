#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsGlobalSettingsWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsGlobalSettingsWatcher(
        CameraSettingsDialogStore* store, QObject* parent = nullptr);
};

} // namespace nx::vms::client::desktop
