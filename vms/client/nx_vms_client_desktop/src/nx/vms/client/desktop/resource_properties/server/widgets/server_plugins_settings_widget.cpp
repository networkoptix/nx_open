#include "server_plugins_settings_widget.h"

#include <QtQuick/QQuickItem>

#include <nx/utils/log/assert.h>

#include "../redux/server_settings_dialog_store.h"

namespace nx::vms::client::desktop {

ServerPluginsSettingsWidget::ServerPluginsSettingsWidget(
    ServerSettingsDialogStore* store,
    QQmlEngine* engine,
    QWidget* parent)
    :
    base_type(engine, parent)
{
    setClearColor(palette().window().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl("Nx/Dialogs/ServerSettings/PluginsInformation.qml"));

    if (!NX_ASSERT(rootObject()))
        return;

    rootObject()->setProperty("store", QVariant::fromValue(store));
}

} // namespace nx::vms::client::desktop
