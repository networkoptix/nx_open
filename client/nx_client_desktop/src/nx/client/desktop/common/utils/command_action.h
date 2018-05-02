#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

class QAction;

namespace nx {
namespace client {
namespace desktop {

using CommandAction = QAction;

using CommandActionPtr = QSharedPointer<CommandAction>;

using CommandActionList = QList<CommandActionPtr>;

} // namespace desktop
} // namespace client
} // namespace nx
