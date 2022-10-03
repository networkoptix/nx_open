// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_manager.h"

#include <QtCore/QPointer>

#include <nx/utils/log/log.h>
#include <nx/utils/range_adapters.h>
#include <nx/utils/thread/mutex.h>

namespace nx::core::access {

using namespace nx::vms::api;

struct AccessRightsManager::Private
{
    QHash<QnUuid, ResourceAccessMap> ownAccessMaps;
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

ResourceAccessMap AccessRightsManager::ownResourceAccessMap(const QnUuid& subjectId) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->ownAccessMaps.value(subjectId);
}

void AccessRightsManager::resetAccessRights(const QHash<QnUuid, ResourceAccessMap>& accessRights)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        d->ownAccessMaps = accessRights;
    }

    NX_DEBUG(this, "Access rights are reset; %1 subjects are defined", accessRights.size());
    if (!accessRights.empty())
        NX_VERBOSE(this, accessRights);

    emit accessRightsReset();
}

void AccessRightsManager::setOwnResourceAccessMap(const QnUuid& subjectId,
    const ResourceAccessMap& value)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);

        const bool existing = d->ownAccessMaps.contains(subjectId);
        auto& accessMapRef = d->ownAccessMaps[subjectId];

        if (existing && accessMapRef == value)
            return;

        accessMapRef = value;
    }

    NX_DEBUG(this, "Access rights are set for subject %1", subjectId);
    NX_VERBOSE(this, value);

    emit ownAccessRightsChanged({subjectId});
}

void AccessRightsManager::removeSubjects(const QSet<QnUuid>& subjectIds)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);

        for (const auto id: subjectIds)
            d->ownAccessMaps.remove(id);
    }

    NX_DEBUG(this, "Access rights are cleared for %1 subjects: %2", subjectIds.size(), subjectIds);
    emit ownAccessRightsChanged(subjectIds);
}

void AccessRightsManager::clear()
{
    resetAccessRights({});
}

} // namespace nx::core::access
