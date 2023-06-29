// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "../field_types.h"

namespace nx::vms::rules::utils {

QnUserResourceSet users(
    const UuidSelection& selection,
    common::SystemContext* context,
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

} // namespace nx::vms::rules::utils
