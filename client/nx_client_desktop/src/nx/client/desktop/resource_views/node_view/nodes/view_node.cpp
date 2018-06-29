#include "view_node.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {

NodePtr ViewNode::create(const NodeList& children)
{
    const NodePtr result(new ViewNode());
    for (const auto& child: children)
        result->addNode(child);

    return result;
}

ViewNode::ViewNode()
{
}

ViewNode::~ViewNode()
{
}

void ViewNode::addNode(const NodePtr& node)
{
    node->setParent(parentForChildren());
    m_nodes.append(node);
}

int ViewNode::childrenCount() const
{
    return m_nodes.size();
}

NodePtr ViewNode::nodeAt(int index) const
{
    return m_nodes.at(index);
}

int ViewNode::indexOf(const NodePtr& node) const
{
    return m_nodes.indexOf(node);
}

QVariant ViewNode::data(int /* column */, int /* role */) const
{
    return QVariant();
}

Qt::ItemFlags ViewNode::flags(int /* column */) const
{
    return Qt::ItemIsEnabled;
}

NodePtr ViewNode::parent() const
{
    return m_parent ? m_parent.lock() : NodePtr();
}

void ViewNode::setParent(const WeakNodePtr& value)
{
    if (m_parent != value)
        m_parent = value;
}

WeakNodePtr ViewNode::parentForChildren()
{
    const auto result = sharedFromThis();
    NX_EXPECT(result, "No shared pointer exists for current node");
    return result.toWeakRef();
}

} // namespace desktop
} // namespace client
} // namespace nx
