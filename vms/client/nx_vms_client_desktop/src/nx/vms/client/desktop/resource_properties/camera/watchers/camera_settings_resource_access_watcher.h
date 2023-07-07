// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;
class SystemContext;

class CameraSettingsResourceAccessWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    CameraSettingsResourceAccessWatcher(
        CameraSettingsDialogStore* store,
        SystemContext* systemContext,
        QObject* parent);
};

} // namespace nx::vms::client::desktop
