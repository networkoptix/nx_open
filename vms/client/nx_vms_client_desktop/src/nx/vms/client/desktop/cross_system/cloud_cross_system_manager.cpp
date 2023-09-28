// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_manager.h"

#include <map>

#include <finders/systems_finder.h>
#include <network/system_helpers.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>

#include "cloud_cross_system_context.h"

namespace nx::vms::client::desktop {

using CloudCrossSystemContextPtr = std::unique_ptr<CloudCrossSystemContext>;

struct CloudCrossSystemManager::Private
{
    std::map<QString, CloudCrossSystemContextPtr> cloudSystems;
};

CloudCrossSystemManager::CloudCrossSystemManager(QObject* parent):
    QObject(parent),
    d(new Private())
{
    const auto setCloudSystems =
        [this](const QnCloudSystemList& currentCloudSystems)
        {
            QSet<QString> newCloudSystemsIds;
            for (const auto& cloudSystem: currentCloudSystems)
                newCloudSystemsIds.insert(helpers::getTargetSystemId(cloudSystem));

            QSet<QString> currentCloudSystemsIds;
            for (const auto& [id, _]: d->cloudSystems)
                currentCloudSystemsIds.insert(id);

            const auto addedCloudIds = newCloudSystemsIds - currentCloudSystemsIds;
            const auto removedCloudIds = currentCloudSystemsIds - newCloudSystemsIds;

            for (const auto& systemId: addedCloudIds)
            {
                auto systemDescription = QnSystemsFinder::instance()->getSystem(systemId);
                NX_VERBOSE(this, "Found new cloud system %1", systemDescription->name());
                d->cloudSystems[systemId] =
                    std::make_unique<CloudCrossSystemContext>(systemDescription);
                emit systemFound(systemId);
            }

            for (const auto& systemId: removedCloudIds)
            {
                NX_VERBOSE(this, "Cloud system %1 is lost", d->cloudSystems[systemId].get());
                d->cloudSystems.erase(systemId);
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
            setCloudSystems(status == core::CloudStatusWatcher::Online
                ? appContext()->cloudStatusWatcher()->cloudSystems()
                : QnCloudSystemList());
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

} // namespace nx::vms::client::desktop
