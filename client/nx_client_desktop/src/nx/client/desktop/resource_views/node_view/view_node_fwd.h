#pragma once

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

namespace nx {
namespace client {
namespace desktop {

class BaseViewNode;
using NodePtr = QSharedPointer<BaseViewNode>;
using WeakNodePtr = QWeakPointer<BaseViewNode>;
using NodeList = QList<NodePtr>;

} // namespace desktop
} // namespace client
} // namespace nx

