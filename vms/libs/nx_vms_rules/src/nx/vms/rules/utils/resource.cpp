// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "../field_types.h"

namespace nx::vms::rules::utils {

QnUserResourceSet users(
    const UuidSelection& selection,
    const common::SystemContext* context,
    bool activeOnly)
{
    auto result = selection.all
        ? nx::utils::toQSet(context->resourcePool()->getResources<QnUserResource>())
        : nx::vms::common::allUsers(context, selection.ids);

    result.remove({});

    if (activeOnly)
        erase_if(result, [](const auto& user) { return !user->isEnabled(); });

    return result;
}

bool isUserSelected(
    const UuidSelection& selection,
    const common::SystemContext* context,
    nx::Uuid userId)
{
    if (selection.all)
        return true;

    if (selection.ids.contains(userId))
        return true;

    for (const auto& user: nx::vms::common::allUsers(context, selection.ids))
    {
        if (user->getId() == userId)
            return true;
    };

    return false;
}

UuidList getResourceIds(const QObject* entity, std::string_view fieldName)
{
    const auto value = entity->property(fieldName.data());

    if (!value.isValid())
        return {};

    UuidList result;

    if (value.canConvert<UuidList>())
        result = value.value<UuidList>();
    else if (value.canConvert<nx::Uuid>())
        result.emplace_back(value.value<nx::Uuid>());

    result.removeAll({});

    return result;
}

} // namespace nx::vms::rules::utils
