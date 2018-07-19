#include "view_node.h"

#include <nx/utils/log/log.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_path.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_data.h>

namespace nx {
namespace client {
namespace desktop {

struct ViewNode::Private
{
    ViewNodeData nodeData;
    WeakNodePtr parent;
    NodeList nodes;
};

//-------------------------------------------------------------------------------------------------

NodePtr ViewNode::create(const ViewNodeData& data)
{
    return NodePtr(new ViewNode(data));
}

NodePtr ViewNode::create(const NodeList& children)
{
    return create(ViewNodeData(), children);
}

NodePtr ViewNode::create(const ViewNodeData& data, const NodeList& children)
{
    const auto result = create(data);
    for (const auto& child: children)
        result->addChild(child);
    return result;
}

ViewNode::ViewNode(const ViewNodeData& data):
    d(new Private())
{
    d->nodeData.applyData(data);
}

ViewNode::~ViewNode()
{
}

bool ViewNode::isLeaf() const
{
    return childrenCount() == 0;
}

int ViewNode::childrenCount() const
{
    return d->nodes.size();
}

const NodeList& ViewNode::children() const
{
    return d->nodes;
}

void ViewNode::addChild(const NodePtr& child)
{
    child->d->parent = currentSharedNode().toStrongRef();
    d->nodes.append(child);
}

NodePtr ViewNode::nodeAt(int index)
{
    return d->nodes.at(index);
}

NodePtr ViewNode::nodeAt(const ViewNodePath& path)
{
    auto currentNode = currentSharedNode().toStrongRef();
    for (const int index: path.indicies())
        currentNode = currentNode->nodeAt(index);
    return currentNode;
}

ViewNodePath ViewNode::path() const
{
    const auto parentNode = parent();
    if (!parentNode)
        return ViewNodePath();

    auto currentPath = parentNode->path();
    currentPath.appendIndex(parentNode->indexOf(currentSharedNode()));
    return currentPath;
}

int ViewNode::indexOf(const ConstNodePtr& node) const
{
    const auto count = d->nodes.size();
    for (int i = 0; i != count; ++i)
    {
        if (d->nodes.at(i).data() == node.data())
            return i;
    }
    return -1;
}

QVariant ViewNode::data(int column, int role) const
{
    return d->nodeData.data(column, role);
}

Qt::ItemFlags ViewNode::flags(int column) const
{
    return d->nodeData.flags(column);
}

NodePtr ViewNode::parent() const
{
    return d->parent ? d->parent.lock() : NodePtr();
}

const ViewNodeData& ViewNode::nodeData() const
{
    return d->nodeData;
}

void ViewNode::applyNodeData(const ViewNodeData& data)
{
    d->nodeData.applyData(data);
}

WeakNodePtr ViewNode::currentSharedNode()
{
    const auto result = sharedFromThis();
    NX_EXPECT(result, "No shared pointer exists for current node");
    return result.toWeakRef();
}

ConstWeakNodePtr ViewNode::currentSharedNode() const
{
    const auto result = sharedFromThis();
    NX_EXPECT(result, "No shared pointer exists for current node");
    return result.toWeakRef();
}


uint qHash(const nx::client::desktop::ViewNodePath& path)
{
    return qHash(path.indicies());
}

} // namespace desktop
} // namespace client
} // namespace nx

