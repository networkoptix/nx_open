#include "view_node.h"

#include <nx/utils/log/log.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_path.h>

namespace nx {
namespace client {
namespace desktop {

struct ViewNode::PathInternal
{
    QVector<int> indicies;
};

//-------------------------------------------------------------------------------------------------

struct ViewNode::Private
{
    ViewNode::Data nodeData;
    WeakNodePtr parent;
    NodeList nodes;
};

//-------------------------------------------------------------------------------------------------

NodePtr ViewNode::create(const Data& data)
{
    return NodePtr(new ViewNode(data));
}

NodePtr ViewNode::create(const NodeList& children)
{
    return create(Data(), children);
}

NodePtr ViewNode::create(const Data& data, const NodeList& children)
{
    const auto result = create(data);
    for (const auto& child: children)
        result->addChild(child);
    return result;
}

ViewNode::ViewNode(const Data& data):
    d(new Private())
{
    setNodeData(data);
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
    const auto columnIt = d->nodeData.data.find(column);
    if (columnIt == d->nodeData.data.end())
        return QVariant();

    const auto& columnData = columnIt.value();
    const auto dataIt = columnData.find(role);
    if (dataIt == columnData.end())
        return QVariant();

    return dataIt.value();
}

Qt::ItemFlags ViewNode::flags(int column) const
{
    const auto flagIt = d->nodeData.flags.find(column);
    if (flagIt == d->nodeData.flags.end())
        return Qt::ItemIsEnabled;

    static constexpr Qt::ItemFlags kCheckableFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    return column == node_view::checkMarkColumn
        ? flagIt.value() | kCheckableFlags
        : flagIt.value();
}

NodePtr ViewNode::parent() const
{
    return d->parent ? d->parent.lock() : NodePtr();
}

bool ViewNode::checkable() const
{
    const auto it = d->nodeData.data.find(node_view::checkMarkColumn);
    if (it == d->nodeData.data.end())
        return false;

    const auto& roleData = it.value();
    const auto checkedStateIt = roleData.find(Qt::CheckStateRole);
    if (checkedStateIt == roleData.end())
        return false;

    return !checkedStateIt.value().isNull();
}

const ViewNode::Data& ViewNode::nodeData() const
{
    return d->nodeData;
}

void ViewNode::setNodeData(const Data& data)
{
    d->nodeData = data;
}

void ViewNode::applyNodeData(const Data& data)
{
    for (auto it = data.data.begin(); it != data.data.end(); ++it)
    {
        const int column = it.key();
        const auto& roleData = it.value();
        for (auto itRoleData = roleData.begin(); itRoleData != roleData.end(); ++itRoleData)
        {
            const int role = itRoleData.key();
            d->nodeData.data[column][role] = itRoleData.value();
        }
    }

    for (auto it = data.flags.begin(); it != data.flags.end(); ++it)
        d->nodeData.flags[it.key()] = it.value();
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

