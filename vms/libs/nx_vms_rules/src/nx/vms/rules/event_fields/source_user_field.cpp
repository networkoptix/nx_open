// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "source_user_field.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qset.h>

namespace nx::vms::rules {

SourceUserField::SourceUserField(nx::vms::common::SystemContext* context):
    SystemContextAware(context)
{
}

bool SourceUserField::match(const QVariant& value) const
{
    if (acceptAll())
        return true;

    const auto userId = value.value<QnUuid>();
    if (ids().contains(userId))
        return true;

    const auto user = resourcePool()->getResourceById<QnUserResource>(userId);
    if (!user)
        return false;

    return nx::utils::toQSet(user->allUserRoleIds()).intersects(ids());
}

} // namespace nx::vms::rules
