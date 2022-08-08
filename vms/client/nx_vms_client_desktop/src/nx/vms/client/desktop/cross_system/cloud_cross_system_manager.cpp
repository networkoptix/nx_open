// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_cross_system_manager.h"

#include <map>

#include <finders/systems_finder.h>
#include <nx/vms/client/desktop/application_context.h>

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
    const auto systemsFinder = QnSystemsFinder::instance();
    if (!systemsFinder) //< Systems finder won't be available in the test environment.
        return;

    NX_VERBOSE(this, "Initialized");
    auto updateSystemStatus =
        [this](const QString& systemId)
        {
            auto systemDescription = QnSystemsFinder::instance()->getSystem(systemId);
            auto existing = d->cloudSystems.find(systemId);
            if (systemDescription && systemDescription->isCloudSystem())
            {
                if (existing != d->cloudSystems.end())
                {
                    NX_VERBOSE(this, "System %1 became online again", systemDescription->name());
                    existing->second->update(CloudCrossSystemContext::UpdateReason::found);
                }
                else
                {
                    NX_VERBOSE(this, "Found new cloud system %1", systemDescription->name());
                    d->cloudSystems[systemId] =
                        std::make_unique<CloudCrossSystemContext>(systemDescription);
                    d->cloudSystems[systemId]->update(CloudCrossSystemContext::UpdateReason::new_);
                }

                emit systemFound(systemId);
            }
            else if (existing != d->cloudSystems.end()) //< System is lost or not cloud anymore.
            {
                NX_VERBOSE(this, "Cloud system %1 lost", existing->second.get());
                d->cloudSystems[systemId]->update(CloudCrossSystemContext::UpdateReason::lost);
                emit systemLost(systemId);
            }
        };

    auto handleSystemAdded =
        [this, updateSystemStatus](const QnSystemDescriptionPtr& systemDescription)
        {
            updateSystemStatus(systemDescription->id());
            connect(systemDescription.get(), &QnBaseSystemDescription::isCloudSystemChanged,
                this, [this, updateSystemStatus,
                    systemId = systemDescription->id(),
                    name = systemDescription->name()]()
                {
                    NX_VERBOSE(this, "Cloud status is changed for %1", name);
                    updateSystemStatus(systemId);
                });
        };

    connect(systemsFinder,
        &QnSystemsFinder::systemDiscovered,
        this,
        handleSystemAdded);

    connect(systemsFinder,
        &QnSystemsFinder::systemLost,
        this,
        updateSystemStatus);

    const auto systemsDescriptions = qnSystemsFinder->systems();
    for (const auto& systemDescription: systemsDescriptions)
        handleSystemAdded(systemDescription);
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

