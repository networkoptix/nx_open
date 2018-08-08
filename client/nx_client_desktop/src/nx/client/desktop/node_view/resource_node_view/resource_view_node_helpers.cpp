#include "resource_view_node_helpers.h"

#include "resource_node_view_constants.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_data.h"
#include "../details/node/view_node_data_builder.h"
#include "../details/node/view_node_helpers.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <ui/style/resource_icon_cache.h>


namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::details;

ViewNodeData getResourceNodeData(
    const QnResourcePtr& resource,
    bool checkable,
    const ChildrenCountExtraTextGenerator& extraTextGenerator = ChildrenCountExtraTextGenerator(),
    int count = 0)
{
    auto data = ViewNodeDataBuilder()
        .withText(resourceNameColumn, resource->getName())
        .withIcon(resourceNameColumn, qnResIconCache->icon(resource))
        .data();

    if (checkable)
        ViewNodeDataBuilder(data).withCheckedState(resourceCheckColumn, Qt::Unchecked);

    const auto resourceData = QVariant::fromValue(resource);
    data.setData(resourceNameColumn, resourceRole, resourceData);
    if (count > 0 && extraTextGenerator)
    {
        data.setData(resourceNameColumn, resourceExtraTextRole,
            extraTextGenerator(count));
    }
    return data;
}

bool isCheckedResourceNode(const NodePtr& node)
{
    return node->data(resourceCheckColumn, Qt::CheckStateRole)
        .value<Qt::CheckState>() == Qt::Checked;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    bool checkable)
{
    return resource ? ViewNode::create(getResourceNodeData(resource, checkable)) : NodePtr();
}

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction,
    bool checkable,
    const ChildrenCountExtraTextGenerator& extraTextGenerator)
{
    const auto pool = qnClientCoreModule->commonModule()->resourcePool();

    const NodeCreationFunction creationFunction = nodeCreationFunction
        ? nodeCreationFunction
        : [](const QnResourcePtr& resource) -> NodePtr { return createResourceNode(resource); };

    NodeList children;
    for (const auto childResource: pool->getResources())
    {
        if (!relationCheckFunction(resource, childResource))
            continue;

        if (const auto node = creationFunction(childResource))
            children.append(node);
    }

    const auto data = getResourceNodeData(resource, checkable,
        extraTextGenerator, children.count());
    return ViewNode::create(data, children);
}

QnResourceList getSelectedResources(const NodePtr& node)
{
    if (!node)
        return QnResourceList();

    if (!node->isLeaf())
    {
        QnResourceList result;
        for (const auto child: node->children())
            result += getSelectedResources(child);
        return result;
    }

    if (!isCheckedResourceNode(node))
        return QnResourceList();

    const auto resource = getResource(node);
    return resource ? QnResourceList({resource}) : QnResourceList();
}

QnResourcePtr getResource(const NodePtr& node)
{
    return node
        ? node->data(resourceNameColumn, resourceRole).value<QnResourcePtr>()
        : QnResourcePtr();
}

QnResourcePtr getResource(const QModelIndex& index)
{
    return getResource(nodeFromIndex(index));
}

QString extraText(const QModelIndex& node)
{
    return node.data(resourceExtraTextRole).toString();
}

} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

