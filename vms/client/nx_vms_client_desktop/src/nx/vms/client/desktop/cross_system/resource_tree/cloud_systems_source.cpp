// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_systems_source.h"

#include <QtCore/QVector>

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>

#include "../cloud_cross_system_manager.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

namespace {

QString connectedCloudSystemId()
{
    return appContext()->currentSystemContext()->globalSettings()->cloudSystemId();
}

QVector<QString> getOtherCloudSystemsIds(const QString& thisCloudSystemId)
{
    QVector<QString> result;

    const auto cloudSystemsManager = appContext()->cloudCrossSystemManager();

    for (const auto& systemId: cloudSystemsManager->cloudSystems())
    {
        const auto systemContext = cloudSystemsManager->systemContext(systemId);
        const auto systemDescription = systemContext->systemDescription();

        if (systemId != thisCloudSystemId && !systemDescription->isPending())
            result.append(systemId);
    }

    return result;
}

} // namespace

CloudSystemsSource::CloudSystemsSource()
{
    initializeRequest =
        [this]
        {
            const auto cloudSystemsManager = appContext()->cloudCrossSystemManager();
            const bool isUnitTestsEnvironment = !cloudSystemsManager;
            if (isUnitTestsEnvironment)
                return;

            m_connectionsGuard.add(QObject::connect(
                cloudSystemsManager, &CloudCrossSystemManager::systemFound,
                    [this, cloudSystemsManager](const QString& systemId)
                    {
                        const auto systemContext = cloudSystemsManager->systemContext(systemId);
                        const auto systemDescription = systemContext->systemDescription();

                        if (systemId != connectedCloudSystemId() && !systemDescription->isPending())
                        {
                            NX_VERBOSE(this, "New cloud system added: %1", systemId);
                            (*addKeyHandler)(systemId);
                        }
                    }));

            m_connectionsGuard.add(QObject::connect(
                cloudSystemsManager, &CloudCrossSystemManager::systemLost,
                    [this](const QString& systemId)
                    {
                        NX_VERBOSE(this, "Cloud system lost: %1", systemId);
                        (*removeKeyHandler)(systemId);
                    }));

            setKeysHandler(
                getOtherCloudSystemsIds(connectedCloudSystemId()));
        };
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
