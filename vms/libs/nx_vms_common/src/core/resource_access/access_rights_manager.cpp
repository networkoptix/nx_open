// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_manager.h"

#include <QtCore/QPointer>

#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>

namespace nx::core::access {

using namespace nx::vms::api;
using namespace nx::vms::common;

struct AccessRightsManager::Private
{
    QHash<nx::Uuid, ResourceAccessMap> ownAccessMaps;
    mutable nx::Mutex mutex;
};

// -----------------------------------------------------------------------------------------------
// AccessRightsManager

AccessRightsManager::AccessRightsManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

AccessRightsManager::~AccessRightsManager()
{
    // Required here for forward-declared scoped pointer destruction.
}

ResourceAccessMap AccessRightsManager::ownResourceAccessMap(const nx::Uuid& subjectId) const
{
    if (auto predefinedGroup = PredefinedUserGroups::find(subjectId))
    {
        return ResourceAccessMap{
            predefinedGroup->resourceAccessRights.cbegin(),
            predefinedGroup->resourceAccessRights.cend()};
    }

    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->ownAccessMaps.value(subjectId);
}

void AccessRightsManager::resetAccessRights(const QHash<nx::Uuid, ResourceAccessMap>& accessRights)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        d->ownAccessMaps = accessRights;
    }

    NX_DEBUG(this, "Access rights are reset; %1 subjects are defined", accessRights.size());
    if (!accessRights.empty())
        NX_VERBOSE(this, nx::containerString(nx::utils::constKeyValueRange(accessRights)));

    emit accessRightsReset();
}

bool AccessRightsManager::setOwnResourceAccessMap(const nx::Uuid& subjectId,
    const ResourceAccessMap& value)
{
    if (PredefinedUserGroups::contains(subjectId))
        return false;

    {
        NX_MUTEX_LOCKER lk(&d->mutex);

        if (d->ownAccessMaps.value(subjectId) == value)
            return false;

        d->ownAccessMaps[subjectId] = value;
    }

    NX_DEBUG(this, "Access rights are set for subject %1", subjectId);
    NX_VERBOSE(this, nx::containerString(nx::utils::constKeyValueRange(value)));

    emit ownAccessRightsChanged({subjectId});
    return true;
}

bool AccessRightsManager::removeSubjects(const QSet<nx::Uuid>& subjectIds)
{
    QSet<nx::Uuid> removedSubjectIds;

    {
        NX_MUTEX_LOCKER lk(&d->mutex);

        for (const auto id: subjectIds)
        {
            if (d->ownAccessMaps.remove(id))
                removedSubjectIds.insert(id);
        }
    }

    if (removedSubjectIds.empty())
        return false;

    NX_DEBUG(this, "Access rights are cleared for %1 subjects: %2",
        removedSubjectIds.size(), nx::containerString(removedSubjectIds));

    emit ownAccessRightsChanged(removedSubjectIds);
    return true;
}

void AccessRightsManager::clear()
{
    resetAccessRights({});
}

} // namespace nx::core::access
