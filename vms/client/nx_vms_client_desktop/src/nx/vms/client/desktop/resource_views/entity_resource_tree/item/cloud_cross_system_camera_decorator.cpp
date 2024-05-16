// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_camera_decorator.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <client/client_globals.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using NodeType = ResourceTree::NodeType;

CloudCrossSystemCameraDecorator::CloudCrossSystemCameraDecorator(
    entity_item_model::AbstractItemPtr sourceItem)
    :
    base_type(),
    m_sourceItem(std::move(sourceItem))
{
    m_sourceItem->setDataChangedCallback(
        [this](const QVector<int>& roles) { notifyDataChanged(roles); });
}

QVariant CloudCrossSystemCameraDecorator::data(int role) const
{
    if (role == Qn::NodeTypeRole)
        return QVariant::fromValue(NodeType::resource);

    return m_sourceItem->data(role);
}

Qt::ItemFlags CloudCrossSystemCameraDecorator::flags() const
{
    auto result = m_sourceItem->flags();

    const auto resource = m_sourceItem->data(core::ResourceRole).value<QnResourcePtr>();
    if (!NX_ASSERT(!resource.isNull(), "Resource node expected"))
        return result;

    result.setFlag(Qt::ItemIsEditable, false);
    result.setFlag(Qt::ItemIsDragEnabled, true);
    result.setFlag(Qt::ItemIsDropEnabled, false);

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
