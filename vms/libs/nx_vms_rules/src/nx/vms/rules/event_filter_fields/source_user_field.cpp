// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_field.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

namespace nx::vms::rules {

SourceUserField::SourceUserField(nx::vms::common::SystemContext* context):
    SystemContextAware(context)
{
}

bool SourceUserField::match(const QVariant& value) const
{
    if (acceptAll())
        return true;

    const auto userId = value.value<nx::Uuid>();
    if (ids().contains(userId))
        return true;

    const auto user = resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return false;

    return nx::vms::common::userGroupsWithParents(user).intersects(ids());
}

} // namespace nx::vms::rules
