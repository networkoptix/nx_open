// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <map>

#include <QtCore/QString>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

using DebugActionHandler = std::function<void(QnWorkbenchContext*)>;

void registerDebugAction(const QString& name, DebugActionHandler handler);
std::map<QString, DebugActionHandler> debugActions();

} // namespace nx::vms::client::desktop
