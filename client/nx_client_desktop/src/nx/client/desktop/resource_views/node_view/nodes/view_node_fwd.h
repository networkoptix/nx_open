#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

namespace nx {
namespace client {
namespace desktop {

class ViewNode;
class ViewNodePath;

using NodePtr = QSharedPointer<ViewNode>;
using ConstNodePtr = QSharedPointer<const ViewNode>;
using WeakNodePtr = QWeakPointer<ViewNode>;
using ConstWeakNodePtr = QWeakPointer<const ViewNode>;
using NodeList = QList<NodePtr>;

} // namespace desktop
} // namespace client
} // namespace nx

