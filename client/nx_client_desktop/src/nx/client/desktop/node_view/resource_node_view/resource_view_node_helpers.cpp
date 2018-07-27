#include "resource_view_node_helpers.h"

#include "resource_node_view_constants.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <ui/style/resource_icon_cache.h>

#include <nx/client/desktop/resource_views/node_view/node/view_node.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data_builder.h>

namespace {

using namespace nx::client::desktop;
using namespace nx::client::desktop::node_view;
using namespace nx::client::desktop::node_view::helpers;

ViewNodeData getResourceNodeData(
    const QnResourcePtr& resource,
    const OptionalCheckedState& checkedState,
    const ChildrenCountExtraTextGenerator& extraTextGenerator = ChildrenCountExtraTextGenerator(),
    int count = 0)
{
    auto data = ViewNodeDataBuilder()
        .withText(node_view::resourceNameColumn, resource->getName())
        .withCheckedState(node_view::resourceCheckColumn, checkedState)
        .withIcon(node_view::resourceNameColumn, qnResIconCache->icon(resource))
        .data();

    const auto resourceData = QVariant::fromValue(resource);
    data.setData(node_view::resourceNameColumn, node_view::resourceRole, resourceData);
    if (count > 0 && extraTextGenerator)
    {
        data.setData(node_view::resourceNameColumn, node_view::extraTextRole,
            extraTextGenerator(count));
    }
    return data;
}

bool isCheckedResourceNode(const NodePtr& node)
{
    return node->data(node_view::resourceCheckColumn, Qt::CheckStateRole)
        .value<Qt::CheckState>() == Qt::Checked;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace node_view {
namespace helpers {

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const OptionalCheckedState& checkedState)
{
    return resource ? ViewNode::create(getResourceNodeData(resource, checkedState)) : NodePtr();
}

NodePtr createParentResourceNode(
    const QnResourcePtr& resource,
    const RelationCheckFunction& relationCheckFunction,
    const NodeCreationFunction& nodeCreationFunction,
    const OptionalCheckedState& checkedState,
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

    const auto data = getResourceNodeData(resource, checkedState,
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
        ? node->data(node_view::resourceNameColumn, node_view::resourceRole).value<QnResourcePtr>()
        : QnResourcePtr();
}

QnResourcePtr getResource(const QModelIndex& index)
{
    return getResource(nodeFromIndex(index));
}

} // namespace helpers
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

