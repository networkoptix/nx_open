// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_access_rights_manager.h"

namespace nx::core::access {

const QnUuid AbstractAccessRightsManager::kAnyResourceId = QnUuid{};

nx::vms::api::AccessRights AbstractAccessRightsManager::ownAccessRights(
    const QnUuid& subjectId, const QnUuid& targetId) const
{
    return ownResourceAccessMap(subjectId).value(targetId);
}

} // namespace nx::core::access
