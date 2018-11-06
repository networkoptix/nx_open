#include "view_node.h"

#include "view_node_data.h"

#include <nx/utils/log/log.h>
#include <utils/math/math.h>

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

struct ViewNode::Private
{
    ViewNodeData nodeData;
    WeakNodePtr parent;
    NodeList nodes;
};

//-------------------------------------------------------------------------------------------------

NodePtr ViewNode::create()
{
    return NodePtr(new ViewNode({}));
}

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
    return !childrenCount();
}

bool ViewNode::isRoot() const
{
    return parent().isNull();
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

void ViewNode::addChildren(const NodeList& children)
{
    for (const auto child: children)
        addChild(child);
}

void ViewNode::removeChild(int index)
{
    if (!qBetween(0, index, d->nodes.size()))
    {
        NX_ASSERT(false, "Wrong index!");
        return;
    }

    const auto node = d->nodes[index];
    d->nodes.removeAt(index);
}

void ViewNode::removeChild(const NodePtr& child)
{
    removeChild(indexOf(child));
}

NodePtr ViewNode::nodeAt(int index)
{
    if (index >= 0 || index < d->nodes.size())
        return d->nodes.at(index);

    NX_ASSERT(false, "Child node with specified index does not exist!");
    return NodePtr();
}

NodePtr ViewNode::nodeAt(const ViewNodePath& path)
{
    auto currentNode = currentSharedNode().toStrongRef();
    if (path.isEmpty())
        return currentNode;

    for (const int index: path.indices())
    {
        currentNode = currentNode->nodeAt(index);
        if (!currentNode)
            break;
    }
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

QVariant ViewNode::property(int id) const
{
    return d->nodeData.property(id);
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
    NX_ASSERT(result, "No shared pointer exists for current node");
    return result.toWeakRef();
}

ConstWeakNodePtr ViewNode::currentSharedNode() const
{
    const auto result = sharedFromThis();
    NX_ASSERT(result, "No shared pointer exists for current node");
    return result.toWeakRef();
}

uint qHash(const ViewNodePath& path)
{
    return qHash(path.indices());
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop

