#include "resource_view_node_helpers.h"

#include "resource_node_view_constants.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_data.h"
#include "../details/node/view_node_data_builder.h"
#include "../details/node/view_node_helpers.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <ui/style/resource_icon_cache.h>


namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;

ViewNodeData getResourceNodeData(
    const QnResourcePtr& resource,
    const QString& extraText,
    bool checkable)
{
    auto data = ViewNodeDataBuilder()
        .withText(resourceNameColumn, resource->getName())
        .withIcon(resourceNameColumn, qnResIconCache->icon(resource))
        .data();

    if (checkable)
        ViewNodeDataBuilder(data).withCheckedState(resourceCheckColumn, Qt::Unchecked);

    if (!extraText.isEmpty())
        data.setData(resourceNameColumn, resourceExtraTextRole, extraText);

    const auto resourceData = QVariant::fromValue(resource);
    data.setData(resourceNameColumn, resourceRole, resourceData);
    return data;
}

ViewNodeData getGroupNodeData(
    const QnVirtualCameraResourcePtr& cameraResource,
    const QString& extraText,
    bool checkable)
{
    static const int kGroupedDeviceSortOrder = -1; //< Multisensor cameras and recorders on top of the list
    const QIcon icon = cameraResource->isMultiSensorCamera()
        ? qnResIconCache->icon(QnResourceIconCache::MultisensorCamera)
        : qnResIconCache->icon(QnResourceIconCache::Recorder);

    auto data = ViewNodeDataBuilder()
        .withText(resourceNameColumn, cameraResource->getUserDefinedGroupName())
        .withIcon(resourceNameColumn, icon)
        .withGroupSortOrder(kGroupedDeviceSortOrder)
        .data();

    if (checkable)
        ViewNodeDataBuilder(data).withCheckedState(resourceCheckColumn, Qt::Unchecked);

    if (!extraText.isEmpty())
        data.setData(resourceNameColumn, resourceExtraTextRole, extraText);

    return data;
}

bool isCheckedResourceNode(const NodePtr& node)
{
    return node->data(resourceCheckColumn, Qt::CheckStateRole)
        .value<Qt::CheckState>() == Qt::Checked;
}

} // namespace

namespace nx::vms::client::desktop {
namespace node_view {

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const QString& extraText,
    bool checkable)
{
    return resource
        ? ViewNode::create(getResourceNodeData(resource, extraText, checkable))
        : NodePtr();
}

NodePtr createGroupNode(
    const QnVirtualCameraResourcePtr& resource,
    const QString& extraText,
    bool checkable)
{
    return resource
        ? ViewNode::create(getGroupNodeData(resource, extraText, checkable))
        : NodePtr();
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

QString extraText(const QModelIndex& index)
{
    return index.data(resourceExtraTextRole).toString();
}

bool isValidNode(const NodePtr& node)
{
    const auto& data = node->nodeData();
    return !data.hasProperty(validResourceProperty)
        || data.property(validResourceProperty).toBool();
}

bool isValidNode(const QModelIndex& index)
{
    const auto node = nodeFromIndex(index);
    return node && isValidNode(node);
}

ViewNodeData getDataForInvalidNode(bool invalid)
{
    ViewNodeData data;
    data.setProperty(validResourceProperty, !invalid);
    return data;
}

void setNodeValidState(const details::NodePtr& node, bool valid)
{
    node->applyNodeData(getDataForInvalidNode(!valid));
}

} // namespace node_view
} // namespace nx::vms::client::desktop

