#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

#include <nx/utils/uuid.h>
#include <nx/utils/std/optional.h>

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace details {

class ViewNode;
class ViewNodePath;

using NodePtr = QSharedPointer<ViewNode>;
using ConstNodePtr = QSharedPointer<const ViewNode>;
using WeakNodePtr = QWeakPointer<ViewNode>;
using ConstWeakNodePtr = QWeakPointer<const ViewNode>;
using NodeList = QList<NodePtr>;

using OptionalCheckedState = std::optional<Qt::CheckState>;
using ColumnSet = QSet<int>;
using UuidSet = QSet<QnUuid>;

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

