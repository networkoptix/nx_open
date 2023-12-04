// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_access_rights_manager.h"

#include <optional>

#include <QtCore/QPointer>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace nx::core::access;

struct ProxyAccessRightsManager::Private
{
    const QPointer<AbstractAccessRightsManager> sourceManager;
    QnUuid currentSubjectId;
    std::optional<ResourceAccessMap> accessMapOverride;
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
            NX_VERBOSE(this, "Access rights changed remotely for subject %1", subjectIds);

            if (d->accessMapOverride && NX_ASSERT(!d->currentSubjectId.isNull()))
            {
                if (subjectIds.contains(d->currentSubjectId))
                {
                    NX_VERBOSE(this, "Current subject %2 is changed locally, remote changes will "
                        "be ignored", d->currentSubjectId);
                }

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
            NX_VERBOSE(this, "Access rights reset");
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

    if (value == ownResourceAccessMap(d->currentSubjectId))
        return;

    emit proxyAccessRightsAboutToBeChanged();

    NX_VERBOSE(this, "Access rights changed locally for %1", d->currentSubjectId);

    d->accessMapOverride = value;

    emit ownAccessRightsChanged({d->currentSubjectId});
    emit proxyAccessRightsChanged();
}

void ProxyAccessRightsManager::resetOwnResourceAccessMap()
{
    if (!NX_ASSERT(!d->currentSubjectId.isNull()) || !d->accessMapOverride)
        return;

    NX_VERBOSE(this, "Local access rights changes are cancelled for %1", d->currentSubjectId);

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
