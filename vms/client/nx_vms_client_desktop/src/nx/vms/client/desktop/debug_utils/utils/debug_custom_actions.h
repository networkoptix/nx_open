#pragma once

#include <functional>

#include <QtCore/QMap>
#include <QtCore/QString>

namespace nx::vms::client::desktop {

using DebugActionHandler = std::function<void()>;

void registerDebugAction(const QString& name, DebugActionHandler handler);
QMap<QString, DebugActionHandler> debugActions();

} // namespace nx::vms::client::desktop
