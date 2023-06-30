// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_priority_decorator_model.h"

#include <utility>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>

namespace nx::vms::client::desktop {

FailoverPriorityDecoratorModel::FailoverPriorityDecoratorModel(QObject* parent /*= nullptr*/):
    base_type(parent)
{
}

QVariant FailoverPriorityDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (role == ResourceDialogItemRole::FailoverPriorityRole)
    {
        const auto camera = index.siblingAtColumn(failover_priority_view::ResourceColumn)
            .data(core::ResourceRole)
            .value<QnResourcePtr>().dynamicCast<QnVirtualCameraResource>();

        if (camera.isNull())
            return {};

        return QVariant::fromValue(
            m_failoverPriorityOverride.value(camera, camera->failoverPriority()));
    }

    return base_type::data(index, role);
}

FailoverPriorityDecoratorModel::FailoverPriorityMap
    FailoverPriorityDecoratorModel::failoverPriorityOverride() const
{
    return m_failoverPriorityOverride;
}

void FailoverPriorityDecoratorModel::setFailoverPriorityOverride(
    const QModelIndexList& indexes,
    nx::vms::api::FailoverPriority failoverPriority)
{
    for (const auto& index: indexes)
    {
        if (!index.isValid() || index.model() != this)
        {
            NX_ASSERT(false, "Invalid index.");
            continue;
        }

        const auto cameraResource = index.data(core::ResourceRole).value<QnResourcePtr>()
            .dynamicCast<QnVirtualCameraResource>();

        if (!cameraResource)
            continue;

        const auto failoverIndex =
            index.siblingAtColumn(failover_priority_view::FailoverPriorityColumn);
        if (cameraResource->failoverPriority() == failoverPriority)
        {
            m_failoverPriorityOverride.remove(cameraResource);
        }
        else if (!m_failoverPriorityOverride.contains(cameraResource)
            || m_failoverPriorityOverride.value(cameraResource) != failoverPriority)
        {
            m_failoverPriorityOverride.insert(cameraResource, failoverPriority);
        }
    }

    const auto columnCount = failover_priority_view::ColumnCount;
    emit dataChanged(
        index(0, columnCount - 1),
        index(rowCount() - 1, columnCount - 1),
        {ResourceDialogItemRole::FailoverPriorityRole});
}

} // namespace nx::vms::client::desktop
