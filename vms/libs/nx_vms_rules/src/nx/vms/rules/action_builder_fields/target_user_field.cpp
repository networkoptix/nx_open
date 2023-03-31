// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_field.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::rules {

TargetUserField::TargetUserField(common::SystemContext* context):
    common::SystemContextAware(context)
{
}

QnUserResourceSet TargetUserField::users() const
{
    QnUserResourceSet result;

    if (acceptAll())
    {
        result = nx::utils::toQSet(resourcePool()->getResources<QnUserResource>());
    }
    else
    {
        QnUserResourceList users;
        QnUuidList groupIds;
        nx::vms::common::getUsersAndGroups(systemContext(), ids(), users, groupIds);

        result = nx::utils::toQSet(users);

        const auto groupUsers = systemContext()->accessSubjectHierarchy()->usersInGroups(
            nx::utils::toQSet(groupIds));

        for (const auto& user: groupUsers)
            result.insert(user);
    }

    erase_if(result, [](const auto& user) { return !user || !user->isEnabled(); });

    return result;
}

} // namespace nx::vms::rules
