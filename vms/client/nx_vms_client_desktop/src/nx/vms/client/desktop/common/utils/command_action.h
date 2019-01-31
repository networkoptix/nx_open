#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

class QAction;

namespace nx::vms::client::desktop {

using CommandAction = QAction;

using CommandActionPtr = QSharedPointer<CommandAction>;

using CommandActionList = QList<CommandActionPtr>;

} // namespace nx::vms::client::desktop
