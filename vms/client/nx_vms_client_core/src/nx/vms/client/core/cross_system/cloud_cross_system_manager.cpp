// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_manager.h"

#include <map>

#include <network/system_helpers.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/system_finder/system_finder.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>

#include "cloud_cross_system_context.h"

namespace nx::vms::client::core {

struct CloudCrossSystemManager::Private
{
    CloudCrossSystemManager* const q;
    nx::utils::ScopedConnections connections;
    std::map<QString, CloudCrossSystemContextPtr> cloudSystems;
    std::unique_ptr<CloudCrossSystemRequestScheduler> scheduler;

    void setCloudSystems(const QnCloudSystemList& currentCloudSystems);
    void requestCloudAuthorization();
};

// ------------------------------------------------------------------------------------------------
// CloudCrossSystemManager

CloudCrossSystemManager::CloudCrossSystemManager(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
    if (ini().disableCrossSiteConnections)
        return;

    d->scheduler = std::make_unique<CloudCrossSystemRequestScheduler>();

    d->connections << connect(
        appContext()->cloudStatusWatcher(), &core::CloudStatusWatcher::statusChanged, this,
        [this](auto status)
        {
            NX_VERBOSE(this, "Cloud status changed to %1", status);

            // Leaving system list as is in case of Cloud going offline.
            if (status == core::CloudStatusWatcher::Online)
            {
                d->setCloudSystems(appContext()->cloudStatusWatcher()->cloudSystems());
            }
            else if (status == core::CloudStatusWatcher::LoggedOut)
            {
                d->setCloudSystems({});
                d->scheduler->reset();
            }
        });

    d->connections << connect(
        appContext()->cloudStatusWatcher(), &core::CloudStatusWatcher::cloudSystemsChanged, this,
        [this](const auto& currentCloudSystems)
        {
            NX_VERBOSE(this, "Cloud systems changed");
            d->setCloudSystems(currentCloudSystems);
        });

    d->setCloudSystems(appContext()->cloudStatusWatcher()->cloudSystems());

    NX_VERBOSE(this, "Initialized");
}

CloudCrossSystemManager::~CloudCrossSystemManager() = default;

QStringList CloudCrossSystemManager::cloudSystems() const
{
    QStringList result;
    for (const auto& [id, _]: d->cloudSystems)
        result.push_back(id);

    return result;
}

CloudCrossSystemContext* CloudCrossSystemManager::systemContext(const QString& systemId) const
{
    auto iter = d->cloudSystems.find(systemId);
    return iter == d->cloudSystems.cend()
        ? nullptr
        : iter->second.get();
}

CloudCrossSystemRequestScheduler* CloudCrossSystemManager::scheduler()
{
    return d->scheduler.get();
}

void CloudCrossSystemManager::setPriority(const QString& systemId, Priority priority)
{
    d->scheduler->setPriority(systemId, (int)priority);
}

void CloudCrossSystemManager::resetCloudSystems(bool enableCloudSystems)
{
    if (enableCloudSystems)
    {
        d->setCloudSystems(appContext()->cloudStatusWatcher()->cloudSystems());
    }
    else
    {
        d->setCloudSystems({});
        d->scheduler->reset();
    }
}

// ------------------------------------------------------------------------------------------------
// CloudCrossSystemManager::Private

void CloudCrossSystemManager::Private::setCloudSystems(
    const QnCloudSystemList& currentCloudSystems)
{
    QSet<QString> newCloudSystemsIds;
    for (const auto& cloudSystem: currentCloudSystems)
        newCloudSystemsIds.insert(helpers::getTargetSystemId(cloudSystem));

    QSet<QString> currentCloudSystemsIds;
    for (const auto& [id, _]: cloudSystems)
        currentCloudSystemsIds.insert(id);

    const auto addedCloudIds = newCloudSystemsIds - currentCloudSystemsIds;
    const auto removedCloudIds = currentCloudSystemsIds - newCloudSystemsIds;

    for (const auto& systemId: addedCloudIds)
    {
        auto systemDescription = appContext()->systemFinder()->getSystem(systemId);
        if (!systemDescription)
            continue;

        NX_VERBOSE(this, "Found new cloud system %1", systemDescription);
        auto context = std::make_unique<CloudCrossSystemContext>(systemDescription);
        connections << connect(
            context.get(), &CloudCrossSystemContext::cloudAuthorizationRequested, q,
            [this]() { requestCloudAuthorization(); });

        cloudSystems[systemId] = std::move(context);
        emit q->systemFound(systemId);
    }

    for (const auto& systemId: removedCloudIds)
    {
        NX_VERBOSE(this, "Cloud system %1 is lost", cloudSystems[systemId].get());
        emit q->systemAboutToBeLost(systemId);
        cloudSystems.erase(systemId);
        emit q->systemLost(systemId);
    }
}

void CloudCrossSystemManager::Private::requestCloudAuthorization()
{
    emit q->cloudAuthorizationRequested(QPrivateSignal{});
}

} // namespace nx::vms::client::core
