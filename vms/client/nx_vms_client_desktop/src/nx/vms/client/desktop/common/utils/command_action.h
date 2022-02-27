// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

class QAction;

namespace nx::vms::client::desktop {

using CommandAction = QAction;

using CommandActionPtr = QSharedPointer<CommandAction>;

using CommandActionList = QList<CommandActionPtr>;

} // namespace nx::vms::client::desktop
