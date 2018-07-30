#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

#include <boost/optional.hpp>

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

using OptionalCheckedState = boost::optional<Qt::CheckState>;
using ColumnsSet = QSet<int>;

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

