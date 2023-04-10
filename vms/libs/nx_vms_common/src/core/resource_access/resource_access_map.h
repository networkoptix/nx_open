// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <nx/utils/uuid.h>
#include <nx/vms/api/types/access_rights_types.h>

class QnResourcePool;

namespace nx::core::access {

using AccessRight = nx::vms::api::AccessRight;
using AccessRights = nx::vms::api::AccessRights;

using ResourceAccessMap = QHash<QnUuid /*resourceId*/, AccessRights>;

/** Merges two resource access maps. */
NX_VMS_COMMON_API ResourceAccessMap& operator+=(
    ResourceAccessMap& destination, const ResourceAccessMap& source);

extern NX_VMS_COMMON_API ResourceAccessMap kFullResourceAccessMap;

NX_VMS_COMMON_API QString toString(const ResourceAccessMap& accessMap,
    QnResourcePool* resourcePool = nullptr, bool multiLine = false);

} // namespace nx::core::access
