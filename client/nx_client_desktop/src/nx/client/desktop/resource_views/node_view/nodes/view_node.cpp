#include "view_node.h"

#include <nx/utils/log/log.h>

namespace {

Qt::ItemFlags getFlags(bool checkable)
{
    static constexpr Qt::ItemFlags baseFlags = Qt::ItemIsEnabled;
    return checkable
        ? baseFlags | Qt::ItemIsUserCheckable | Qt::ItemIsEditable
        : baseFlags;
}

} // namespace

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
    {
        child->d->parent = result;
        result->d->nodes.append(child);
    }
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

int ViewNode::childrenCount() const
{
    return d->nodes.size();
}

const NodeList& ViewNode::children() const
{
    return d->nodes;
}

NodePtr ViewNode::nodeAt(int index) const
{
    return d->nodes.at(index);
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
    return d->nodes.indexOf(node);
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
    return flagIt == d->nodeData.flags.end() ? Qt::ItemIsEnabled : flagIt.value();
}

NodePtr ViewNode::parent() const
{
    return d->parent ? d->parent.lock() : NodePtr();
}

bool ViewNode::checkable() const
{
    return d->nodeData.checkable;
}

Qt::CheckState ViewNode::checkedState() const
{
    return d->nodeData.checkedState;
}

const ViewNode::Data& ViewNode::nodeData() const
{
    return d->nodeData;
}

void ViewNode::setNodeData(const Data& data)
{
    d->nodeData = data;
    d->nodeData.flags[CheckMarkColumn] = getFlags(checkable());
    d->nodeData.data[CheckMarkColumn][Qt::CheckStateRole] = checkable()
        ? d->nodeData.checkedState
        : QVariant();
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
