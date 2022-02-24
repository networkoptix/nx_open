// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
