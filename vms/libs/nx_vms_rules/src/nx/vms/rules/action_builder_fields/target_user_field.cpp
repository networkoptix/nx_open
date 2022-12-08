// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "target_user_field.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::rules {

TargetUserField::TargetUserField(common::SystemContext* context):
    common::SystemContextAware(context)
{
}

QnUserResourceList TargetUserField::users() const
{
    if (acceptAll())
        return resourcePool()->getResources<QnUserResource>();

    QnUserResourceList users;
    QList<QnUuid> roles;

    systemContext()->userRolesManager()->usersAndRoles(ids(), users, roles);

    if (roles.empty())
        return users;

    QnUserResourceSet usersInRoles;
    for (const auto& role: roles)
    {
        for (const auto& subject: systemContext()->resourceAccessSubjectsCache()->allUsersInRole(role))
            usersInRoles.insert(subject.user());
    }

    usersInRoles += QnUserResourceSet{users.cbegin(), users.cend()};

    return usersInRoles.values();
}

} // namespace nx::vms::rules
