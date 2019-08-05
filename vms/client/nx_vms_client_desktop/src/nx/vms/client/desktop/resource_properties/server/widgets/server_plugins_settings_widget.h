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
