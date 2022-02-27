// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "failover_priority_decorator_model.h"

#include <utility>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>

namespace nx::vms::client::desktop {

FailoverPriorityDecoratorModel::FailoverPriorityDecoratorModel(QObject* parent /*= nullptr*/):
    base_type(parent)
{
}

QVariant FailoverPriorityDecoratorModel::data(const QModelIndex& index, int role) const
{
    if (role == Qn::FailoverPriorityRole)
    {
        const auto camera = index.data(Qn::ResourceRole).value<QnResourcePtr>()
            .dynamicCast<QnVirtualCameraResource>();

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

        const auto cameraResource = index.data(Qn::ResourceRole).value<QnResourcePtr>()
            .dynamicCast<QnVirtualCameraResource>();

        if (!cameraResource)
            continue;

        if (cameraResource->failoverPriority() == failoverPriority)
        {
            if (m_failoverPriorityOverride.remove(cameraResource) != 0)
                emit dataChanged(index, index, {Qn::FailoverPriorityRole});
        }
        else if (!m_failoverPriorityOverride.contains(cameraResource)
            || m_failoverPriorityOverride.value(cameraResource) != failoverPriority)
        {
            m_failoverPriorityOverride.insert(cameraResource, failoverPriority);
            emit dataChanged(index, index, {Qn::FailoverPriorityRole});
        }
    }
}

} // namespace nx::vms::client::desktop
