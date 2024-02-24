// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsEngineLicenseWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit CameraSettingsEngineLicenseWatcher(
        CameraSettingsDialogStore* store, SystemContext* context, QObject* parent);
};

} // namespace nx::vms::client::desktop
