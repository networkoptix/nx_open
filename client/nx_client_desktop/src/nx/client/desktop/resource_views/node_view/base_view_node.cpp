#include "base_view_node.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace desktop {

NodePtr BaseViewNode::create(const NodeList& children)
{
    const NodePtr result(new BaseViewNode());
    for (const auto& child: children)
        result->addNode(child);

    return result;
}

BaseViewNode::BaseViewNode()
{
}

BaseViewNode::~BaseViewNode()
{
}

void BaseViewNode::addNode(const NodePtr& node)
{
    node->setParent(parentForChildren());
    m_nodes.append(node);
}

int BaseViewNode::childrenCount() const
{
    return m_nodes.size();
}

NodePtr BaseViewNode::nodeAt(int index) const
{
    return m_nodes.at(index);
}

int BaseViewNode::indexOf(const NodePtr& node) const
{
    return m_nodes.indexOf(node);
}

QVariant BaseViewNode::data(int /* column */, int /* role */) const
{
    return QVariant();
}

NodePtr BaseViewNode::parent() const
{
    return m_parent ? m_parent.lock() : NodePtr();
}

void BaseViewNode::setParent(const WeakNodePtr& value)
{
    if (m_parent != value)
        m_parent = value;
}

WeakNodePtr BaseViewNode::parentForChildren()
{
    const auto result = sharedFromThis();
    NX_EXPECT(result, "No shared pointer exists for current node");
    return result.toWeakRef();
}

} // namespace desktop
} // namespace client
} // namespace nx
