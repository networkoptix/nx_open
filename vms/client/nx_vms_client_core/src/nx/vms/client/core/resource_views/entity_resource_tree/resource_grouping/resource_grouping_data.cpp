// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_grouping_data.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>

namespace nx::vms::client::core {
namespace entity_resource_tree {
namespace resource_grouping {

QString getRecorderCameraGroupId(const QnResourcePtr& resource, int /*order*/)
{
    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        return camera->getGroupId();
    return QString();
}

QString getUserDefinedGroupId(const QnResourcePtr& resource, int order)
{
    const auto compositeGroupId = resource_grouping::resourceCustomGroupId(resource);

    if (resource_grouping::compositeIdDimension(compositeGroupId) <= order)
        return QString();

    return resource_grouping::trimCompositeId(compositeGroupId, order + 1);
}

} // namespace resource_grouping
} // namespace entity_resource_tree
} // namespace nx::vms::client::mobile
