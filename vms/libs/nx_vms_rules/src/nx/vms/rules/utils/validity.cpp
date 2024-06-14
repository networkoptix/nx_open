// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "validity.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>

#include "../strings.h"

namespace nx::vms::rules::utils {

ValidationResult layoutValidity(
    common::SystemContext* context,
    const QnLayoutResourcePtr& layout,
    const QnUserResourceList& users)
{
    if (users.empty())
        return {};

    if (!layout->isShared())
    {
        if (users.size() == 1 && layout->getParentId() == (*users.cbegin())->getId())
            return {};

        const auto layoutOwner =
            context->resourcePool()->getResourceById<QnUserResource>(layout->getParentId());
        if (!NX_ASSERT(layoutOwner))
            return {QValidator::State::Invalid};

        return {
            QValidator::State::Invalid,
            Strings::layoutCanOnlyBeShownToOwner(layoutOwner->getName())};
    }

    const auto accessManager = context->resourceAccessManager();
    int usersWithoutAccessCount{0};
    for (const auto& user: users)
    {
        if (!accessManager->hasPermission(user, layout, Qn::ReadPermission))
            ++usersWithoutAccessCount;
    }

    if (usersWithoutAccessCount == 0)
        return{};

    return {
        QValidator::State::Intermediate,
        Strings::usersHaveNoAccessToLayout(usersWithoutAccessCount == users.size())};
}

} // namespace nx::vms::rules::utils
