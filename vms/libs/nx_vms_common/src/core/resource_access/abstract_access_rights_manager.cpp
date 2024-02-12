// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_access_rights_manager.h"

namespace nx::core::access {

nx::vms::api::AccessRights AbstractAccessRightsManager::ownAccessRights(
    const nx::Uuid& subjectId, const nx::Uuid& targetId) const
{
    return ownResourceAccessMap(subjectId).value(targetId);
}

} // namespace nx::core::access
