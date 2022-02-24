// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct ServerSettingsDialogState;
class ServerSettingsDialogStore;

class ServerPluginsSettingsWidget: public QQuickWidget
{
    using base_type = QQuickWidget;

public:
    ServerPluginsSettingsWidget(
        ServerSettingsDialogStore* store,
        QQmlEngine* engine,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
