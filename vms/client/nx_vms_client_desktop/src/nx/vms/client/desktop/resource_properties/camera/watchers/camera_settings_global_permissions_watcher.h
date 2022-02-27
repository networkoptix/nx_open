// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsGlobalPermissionsWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit CameraSettingsGlobalPermissionsWatcher(
        CameraSettingsDialogStore* store, QObject* parent = nullptr);
};

} // namespace nx::vms::client::desktop
