// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_manager.h"

#include <map>

#include <QtCore/QCoreApplication>

#include <finders/systems_finder.h>
#include <network/base_system_description.h>
#include <network/system_helpers.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/ini.h>

#include "cloud_cross_system_context.h"

namespace nx::vms::client::desktop {

struct CloudCrossSystemManager::Private
{
    static constexpr int kMaxInitialConnections = 20;

    CloudCrossSystemManager* const q;
    std::map<QString, CloudCrossSystemContextPtr> cloudSystems;
    std::map<QString, QnSystemDescriptionPtr> availableCloudSystems;

    void addCloudSystem(QnSystemDescriptionPtr systemDescription);
    void removeCloudSystem(const QString& systemId);
    CloudCrossSystemContext* makeSystemContext(const QString& cloudSystemId);
};

CloudCrossSystemContext* CloudCrossSystemManager::Private::makeSystemContext(
    const QString& cloudSystemId)
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return nullptr;

    // Already connected.
    if (!NX_ASSERT(!cloudSystems.contains(cloudSystemId)))
        return cloudSystems[cloudSystemId].get();

    // Not found - Cloud layout may contain items from other systems that are unavailable.
    if (!availableCloudSystems.contains(cloudSystemId))
        return nullptr;

    cloudSystems[cloudSystemId] =
        std::make_unique<CloudCrossSystemContext>(availableCloudSystems[cloudSystemId]);

    emit q->systemContextReady(cloudSystemId);

    return cloudSystems[cloudSystemId].get();
}

void CloudCrossSystemManager::Private::addCloudSystem(
    QnSystemDescriptionPtr systemDescription)
{
    const auto cloudSystemId = systemDescription->cloudId();
    availableCloudSystems[cloudSystemId] = systemDescription;

    if (cloudSystems.size() < kMaxInitialConnections && systemDescription->isOnline())
        makeSystemContext(cloudSystemId);
}

void CloudCrossSystemManager::Private::removeCloudSystem(const QString &systemId)
{
    cloudSystems.erase(systemId);
    availableCloudSystems.erase(systemId);
}

CloudCrossSystemManager::CloudCrossSystemManager(QObject* parent):
    QObject(parent),
    d(new Private{.q = this})
{
    if (ini().disableCrossSiteConnections)
        return;

    const auto setCloudSystems =
        [this](const QnCloudSystemList& currentCloudSystems)
        {
            QSet<QString> newCloudSystemsIds;
            for (const auto& cloudSystem: currentCloudSystems)
                newCloudSystemsIds.insert(helpers::getTargetSystemId(cloudSystem));

            QSet<QString> currentCloudSystemsIds;
            for (const auto& [id, _]: d->availableCloudSystems)
                currentCloudSystemsIds.insert(id);

            const auto addedCloudIds = newCloudSystemsIds - currentCloudSystemsIds;
            const auto removedCloudIds = currentCloudSystemsIds - newCloudSystemsIds;

            for (const auto& systemId: addedCloudIds)
            {
                auto systemDescription = QnSystemsFinder::instance()->getSystem(systemId);
                if (!systemDescription)
                    continue;

                NX_VERBOSE(this, "Found new cloud system %1", systemDescription);
                d->addCloudSystem(systemDescription);
                emit systemFound(systemId);
            }

            for (const auto& systemId: removedCloudIds)
            {
                NX_VERBOSE(this, "Cloud system %1 is lost", d->cloudSystems[systemId].get());
                d->removeCloudSystem(systemId);
                emit systemLost(systemId);
            }
        };

    connect(
        appContext()->cloudStatusWatcher(),
        &core::CloudStatusWatcher::statusChanged,
        this,
        [this, setCloudSystems](auto status)
        {
            NX_VERBOSE(this, "Cloud status changed to %1", status);

            // Leaving system list as is in case of Cloud going offline.
            if (status == core::CloudStatusWatcher::Online)
                setCloudSystems(appContext()->cloudStatusWatcher()->cloudSystems());
            else if (status == core::CloudStatusWatcher::LoggedOut)
                setCloudSystems({});
        });

    connect(
        appContext()->cloudStatusWatcher(),
        &core::CloudStatusWatcher::cloudSystemsChanged,
        this,
        [this, setCloudSystems](const auto& currentCloudSystems)
        {
            NX_VERBOSE(this, "Cloud systems changed");
            setCloudSystems(currentCloudSystems);
        });

    setCloudSystems(appContext()->cloudStatusWatcher()->cloudSystems());

    registerDebugAction("Cross-system contexts reset",
        [this, setCloudSystems](auto)
        {
            setCloudSystems({});
        });

    registerDebugAction("Cross-system contexts restore",
        [this, setCloudSystems](auto)
        {
            setCloudSystems(appContext()->cloudStatusWatcher()->cloudSystems());
        });

    NX_VERBOSE(this, "Initialized");
}

CloudCrossSystemManager::~CloudCrossSystemManager() = default;

QStringList CloudCrossSystemManager::cloudSystems() const
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return {};

    QStringList result;
    for (const auto& [id, _]: d->availableCloudSystems)
        result.push_back(id);

    return result;
}

CloudCrossSystemContext* CloudCrossSystemManager::systemContext(const QString& systemId)
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return nullptr;

    auto iter = d->cloudSystems.find(systemId);
    return iter == d->cloudSystems.cend()
        ? d->makeSystemContext(systemId) //< Force the connection if it was deferred.
        : iter->second.get();
}

const QnBaseSystemDescription* CloudCrossSystemManager::systemDescription(
    const QString& systemId) const
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return nullptr;

    return d->availableCloudSystems.at(systemId).get();
}

bool CloudCrossSystemManager::isSystemContextAvailable(const QString& systemId) const
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return false;

    return d->cloudSystems.contains(systemId);
}

bool CloudCrossSystemManager::isConnectionDeferred(const QString& systemId) const
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return false;

    const QnBaseSystemDescription* description = systemDescription(systemId);
    return !isSystemContextAvailable(systemId) && description && description->isOnline();
}

QMetaObject::Connection CloudCrossSystemManager::requestSystemContext(
    const QString& systemId,
    std::function<void(CloudCrossSystemContext*)> callback)
{
    if (!NX_ASSERT(QThread::currentThread() == qApp->thread()))
        return {};

    if (!NX_ASSERT(callback))
        return {};

    if (isSystemContextAvailable(systemId))
    {
        NX_VERBOSE(this, "System context is ready, callback activated: %1", systemId);
        callback(systemContext(systemId));
        return {};
    }

    NX_VERBOSE(this, "Waiting for the context");

    QMetaObject::Connection connection;
    connection = QObject::connect(this, &CloudCrossSystemManager::systemContextReady,
        [this, callback, requestedSystemId = systemId, connection](const QString& systemId)
        {
            NX_VERBOSE(this, "System context is ready, callback activated: %1", systemId);
            if (systemId == requestedSystemId && NX_ASSERT(systemContext(systemId)))
                callback(systemContext(systemId));

            QObject::disconnect(connection);
        });

    return connection;
}

} // namespace nx::vms::client::desktop
