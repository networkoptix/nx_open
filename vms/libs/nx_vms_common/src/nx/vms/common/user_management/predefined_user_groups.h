// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>

#include <core/resource_access/resource_access_map.h>
#include <nx/vms/api/data/user_group_data.h>

namespace nx::vms::common {

class NX_VMS_COMMON_API PredefinedUserGroups
{
    Q_DECLARE_TR_FUNCTIONS(PredefinedUserGroups)
    PredefinedUserGroups() = delete;
    struct Private;

public:
    /** Checks whether a specified id identifies a predefined user group. */
    static bool contains(const nx::Uuid& groupId);

    /** Returns information structure for a predefined user group, if exists. */
    static std::optional<nx::vms::api::UserGroupData> find(const nx::Uuid& predefinedGroupId);

    /** Returns list of all predefined user group information structures. */
    static const nx::vms::api::UserGroupDataList& list();

    /**
     * Returns list of all predefined user group ids.
     * This method defines typical order of predefined user groups,
     * not relying on nx::vms::api::kPredefinedGroupIds type & order.
     * PredefinedUserGroups::list() keeps the same order.
     */
    static const QList<nx::Uuid>& ids();
};

} // namespace nx::vms::common
