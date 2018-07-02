#include "view_node.h"

#include <nx/utils/log/log.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>

namespace nx {
namespace client {
namespace desktop {

struct ViewNode::PathInternal
{
    QVector<int> indicies;
};

//-------------------------------------------------------------------------------------------------

NodePtr ViewNode::create(const NodeList& children, bool checkable)
{
    const NodePtr result(new ViewNode(checkable));
    for (const auto& child: children)
        result->addNode(child);

    return result;
}

ViewNode::ViewNode(bool checkable):
    m_checkable(checkable)
{
}

ViewNode::~ViewNode()
{
}

void ViewNode::addNode(const NodePtr& node)
{
    node->setParent(currentSharedNode());
    m_nodes.append(node);
}

int ViewNode::childrenCount() const
{
    return m_nodes.size();
}

const NodeList& ViewNode::children() const
{
    return m_nodes;
}

NodePtr ViewNode::nodeAt(int index) const
{
    return m_nodes.at(index);
}

NodePtr ViewNode::nodeAt(const Path& path)
{
    auto currentNode = currentSharedNode().toStrongRef();
    for (const int index: path->indicies)
        currentNode = currentNode->nodeAt(index);
    return currentNode;
}

ViewNode::Path ViewNode::path()
{
    const auto parentNode = parent();
    if (!parentNode)
        return Path(new PathInternal());

    auto currentPath = parentNode->path();
    currentPath->indicies.append(parentNode->indexOf(currentSharedNode()));
    return currentPath;
}

int ViewNode::indexOf(const NodePtr& node) const
{
    return m_nodes.indexOf(node);
}

QVariant ViewNode::data(int column, int role) const
{
    return role == Qt::CheckStateRole && column == NodeViewColumn::CheckMark && checkable()
        ? checkedState()
        : QVariant();
}

Qt::ItemFlags ViewNode::flags(int column) const
{
    static constexpr auto baseFlags = Qt::ItemIsEnabled;
    return checkable() && column == NodeViewColumn::CheckMark
        ? baseFlags | Qt::ItemIsUserCheckable | Qt::ItemIsEditable
        : baseFlags;
}

NodePtr ViewNode::parent() const
{
    return m_parent ? m_parent.lock() : NodePtr();
}

bool ViewNode::checkable() const
{
    return m_checkable;
}

Qt::CheckState ViewNode::checkedState() const
{
    return m_checkedState;
}

void ViewNode::setCheckedState(Qt::CheckState value)
{
    m_checkedState = value;
}

void ViewNode::setParent(const WeakNodePtr& value)
{
    if (m_parent != value)
        m_parent = value;
}

WeakNodePtr ViewNode::currentSharedNode()
{
    const auto result = sharedFromThis();
    NX_EXPECT(result, "No shared pointer exists for current node");
    return result.toWeakRef();
}

} // namespace desktop
} // namespace client
} // namespace nx
