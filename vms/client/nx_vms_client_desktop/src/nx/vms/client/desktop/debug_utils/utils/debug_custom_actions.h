#pragma once

#include <functional>

#include <QtCore/QMap>
#include <QtCore/QString>

namespace nx::vms::client::desktop {

void registerDebugAction(const QString& name, std::function<void(void)> handler);
QMap<QString, std::function<void(void)>> debugActions();

} // namespace nx::vms::client::desktop
