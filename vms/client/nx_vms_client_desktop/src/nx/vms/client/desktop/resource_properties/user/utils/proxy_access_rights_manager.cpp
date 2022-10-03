// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_access_rights_manager.h"

#include <optional>

#include <QtCore/QPointer>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace nx::core::access;

struct ProxyAccessRightsManager::Private
{
    const QPointer<AbstractAccessRightsManager> sourceManager;
    QnUuid currentSubjectId;
    std::optional<ResourceAccessMap> accessMapOverride;
    std::optional<bool> adminAccessRights;
};

ProxyAccessRightsManager::ProxyAccessRightsManager(
    AbstractAccessRightsManager* source,
    QObject* parent)
    :
    base_type(parent),
    d(new Private{source})
{
    if (!NX_ASSERT(d->sourceManager))
        return;

    connect(d->sourceManager, &AbstractAccessRightsManager::ownAccessRightsChanged, this,
        [this](QSet<QnUuid> subjectIds)
        {
            if (d->accessMapOverride && NX_ASSERT(!d->currentSubjectId.isNull()))
            {
                // Ignore edited subject changes.
                subjectIds.remove(d->currentSubjectId);
                if (subjectIds.empty())
                    return;
            }

            emit ownAccessRightsChanged(subjectIds);
        });

    connect(d->sourceManager, &AbstractAccessRightsManager::accessRightsReset, this,
        [this]()
        {
            emit accessRightsReset();
        });
}

ProxyAccessRightsManager::~ProxyAccessRightsManager()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnUuid ProxyAccessRightsManager::currentSubjectId() const
{
    return d->currentSubjectId;
}

void ProxyAccessRightsManager::setCurrentSubjectId(const QnUuid& value)
{
    if (d->currentSubjectId == value)
        return;

    const auto oldSubjectId = d->currentSubjectId;
    const auto hadChanges = hasChanges();

    d->currentSubjectId = value;
    d->accessMapOverride = {};
    d->adminAccessRights = {};

    if (hadChanges)
        emit ownAccessRightsChanged({oldSubjectId});
}

ResourceAccessMap ProxyAccessRightsManager::ownResourceAccessMap(const QnUuid& subjectId) const
{
    if (subjectId == d->currentSubjectId && d->accessMapOverride)
        return *d->accessMapOverride;

    return NX_ASSERT(d->sourceManager)
        ? d->sourceManager->ownResourceAccessMap(subjectId)
        : ResourceAccessMap();
}

void ProxyAccessRightsManager::setOwnResourceAccessMap(const ResourceAccessMap& value)
{
    if (!NX_ASSERT(!d->currentSubjectId.isNull()))
        return;

    d->accessMapOverride = value;
    emit ownAccessRightsChanged({d->currentSubjectId});
}

void ProxyAccessRightsManager::resetOwnResourceAccessMap()
{
    if (!NX_ASSERT(!d->currentSubjectId.isNull()) || !d->accessMapOverride)
        return;

    d->accessMapOverride = {};
    emit ownAccessRightsChanged({d->currentSubjectId});
}

bool ProxyAccessRightsManager::hasChanges() const
{
    return d->accessMapOverride
        && NX_ASSERT(!d->currentSubjectId.isNull())
        && NX_ASSERT(d->sourceManager)
        && (*d->accessMapOverride != d->sourceManager->ownResourceAccessMap(d->currentSubjectId));
}

} // namespace nx::vms::client::desktop
