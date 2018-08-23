#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

#include <nx/utils/uuid.h>
#include <nx/utils/std/optional.h>

namespace nx::client::desktop {
namespace node_view::details {

class ViewNode;
class ViewNodePath;
using PathList = QList<ViewNodePath>;
class ViewNodeDataBuilder;
class ViewNodeData;

using NodePtr = QSharedPointer<ViewNode>;
using ConstNodePtr = QSharedPointer<const ViewNode>;
using WeakNodePtr = QWeakPointer<ViewNode>;
using ConstWeakNodePtr = QWeakPointer<const ViewNode>;
using NodeList = QList<NodePtr>;

using OptionalCheckedState = std::optional<Qt::CheckState>;
using ColumnSet = QSet<int>;
using UuidSet = QSet<QnUuid>;
using UuidList = QList<QnUuid>;

} // namespace node_view::details
} // namespace nx::client::desktop
