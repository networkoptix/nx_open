// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_view_node_helpers.h"

#include <QtCore/QModelIndex>
#include <QtGui/QPainter>

#include "resource_node_view_constants.h"
#include "../details/node/view_node.h"
#include "../details/node/view_node_data.h"
#include "../details/node/view_node_data_builder.h"
#include "../details/node/view_node_helper.h"

#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/client_core_module.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>

namespace {

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::node_view;
using namespace nx::vms::client::desktop::node_view::details;
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

    const auto icon = qnResIconCache->icon(resource);
    auto data = ViewNodeDataBuilder()
        .withData(resourceColumn, resourceColumnRole, true)
        .withText(resourceColumn, resource->getName())
        .withIcon(resourceColumn, icon)
        .withData(resourceColumn, resourceExtraStatusRole,
            QVariant::fromValue(getResourceExtraStatus(resource)))
        .data();

    if (!extraText.isEmpty())
        data.setData(resourceColumn, resourceExtraTextRole, extraText);

    const auto resourceData = QVariant::fromValue(resource);
    data.setData(resourceColumn, resourceRole, resourceData);

    return data;
}

QString extraText(const QModelIndex& index)
{
    return index.data(resourceExtraTextRole).toString();
}

} // namespace node_view
} // namespace nx::vms::client::desktop
