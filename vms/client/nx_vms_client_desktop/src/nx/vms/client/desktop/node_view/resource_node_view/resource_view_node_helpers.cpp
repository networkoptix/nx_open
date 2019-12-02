#include "resource_view_node_helpers.h"

#include "resource_node_view_constants.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_data.h"
#include "../details/node/view_node_data_builder.h"
#include "../details/node/view_node_helper.h"

#include <QtGui/QPainter>

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <ui/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/resource_views/data/camera_extra_status.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;

ViewNodeData getResourceNodeDataInternal(
    const QnResourcePtr& resource,
    const QString& extraText,
    bool checkable,
    int primaryColumn)
{
    auto data = getResourceNodeData(resource, primaryColumn, extraText);
    if (checkable)
        ViewNodeDataBuilder(data).withCheckedState(resourceCheckColumn, Qt::Unchecked);

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

bool isResourceColumn(const QModelIndex& index)
{
    const auto node = ViewNodeHelper::nodeFromIndex(index);
    if (!node)
        return false;

    const int column = index.column();
    const auto& data = node->data();
    return data.hasData(column, resourceColumnRole)
        && data.data(column, resourceColumnRole).toBool();
}

ViewNodeData getGroupNodeData(
    const QnVirtualCameraResourcePtr& cameraResource,
    int resourceColumn,
    const QString& extraText,
    int checkColumn)
{
    if (!cameraResource)
        return ViewNodeData();

    static const int kGroupedDeviceSortOrder = -1; //< Multisensor cameras and recorders on top of the list
    const QIcon icon = cameraResource->isMultiSensorCamera()
        ? qnResIconCache->icon(QnResourceIconCache::MultisensorCamera)
        : qnResIconCache->icon(QnResourceIconCache::Recorder);

    auto data = ViewNodeDataBuilder()
        .withData(resourceColumn, resourceColumnRole, true)
        .withText(resourceColumn, cameraResource->getUserDefinedGroupName())
        .withIcon(resourceColumn, icon)
        .withGroupSortOrder(kGroupedDeviceSortOrder)
        .data();

    if (checkColumn >= 0)
        ViewNodeDataBuilder(data).withCheckedState(checkColumn, Qt::Unchecked);

    if (!extraText.isEmpty())
        data.setData(resourceColumn, resourceExtraTextRole, extraText);

    return data;
}

ViewNodeData getResourceNodeData(
    const QnResourcePtr& resource,
    int resourceColumn,
    const QString& extraText)
{
    if (!resource)
    {
        NX_ASSERT(false, "Can't create node data for empty resource!");
        return ViewNodeData();
    }

    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    const auto icon = qnResIconCache->icon(resource);
    auto data = ViewNodeDataBuilder()
        .withData(resourceColumn, resourceColumnRole, true)
        .withText(resourceColumn, resource->getName())
        .withIcon(resourceColumn, icon)
        .withData(resourceColumn, cameraExtraStatusRole,
            QVariant::fromValue(getCameraExtraStatus(camera)))
        .data();

    if (!extraText.isEmpty())
        data.setData(resourceColumn, resourceExtraTextRole, extraText);

    const auto resourceData = QVariant::fromValue(resource);
    data.setData(resourceColumn, resourceRole, resourceData);

    return data;
}

NodePtr createResourceNode(
    const QnResourcePtr& resource,
    const QString& extraText,
    bool checkable)
{
    return resource
        ? ViewNode::create(getResourceNodeDataInternal(resource, extraText, checkable, resourceNameColumn))
        : NodePtr();
}

NodePtr createGroupNode(
    const QnVirtualCameraResourcePtr& resource,
    const QString& extraText,
    bool checkable)
{
    if (!resource)
        return NodePtr();

    return ViewNode::create(getGroupNodeData(
        resource, resourceNameColumn, extraText, checkable ? resourceCheckColumn : -1));
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
    return getResource(ViewNodeHelper::nodeFromIndex(index));
}

QString extraText(const QModelIndex& index)
{
    return index.data(resourceExtraTextRole).toString();
}

bool isValidResourceNode(const NodePtr& node)
{
    const auto& data = node->data();
    return !data.hasProperty(validResourceProperty)
        || data.property(validResourceProperty).toBool();
}

bool isValidResourceNode(const QModelIndex& index)
{
    const auto node = ViewNodeHelper::nodeFromIndex(index);
    return node && isValidResourceNode(node);
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

