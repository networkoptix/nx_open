// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recent_local_systems_finder.h"

#include <network/local_system_description.h>
#include <nx/build_info.h>
#include <nx/network/address_resolver.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

using namespace nx::vms::client::core;

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent): base_type(parent)
{
    connect(&appContext()->coreSettings()->recentLocalConnections,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &QnRecentLocalSystemsFinder::updateSystems);
    connect(&appContext()->coreSettings()->searchAddresses,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &QnRecentLocalSystemsFinder::updateSystems);

    updateSystems();
}

void QnRecentLocalSystemsFinder::updateSystems()
{
    // Connection version is stored in settings starting from 6.0. Actually this version should be
    // fixed during the settings migration, but if user run 6.0 at least once before this change is
    // implemented, assert will be raised. So introducing sane default value here.
    static constexpr nx::utils::SoftwareVersion kSystemVersionWasNotStored{5, 1, 0, 0};

    SystemsHash newSystems;
    const auto connections = appContext()->coreSettings()->recentLocalConnections();
    for (const auto& [id, connection]: connections.asKeyValueRange())
    {
        if (id.isNull() || connection.urls.isEmpty())
            continue;

        const bool hasOnlyCloudUrls = std::all_of(connection.urls.cbegin(), connection.urls.cend(),
            [](const nx::utils::Url& url)
            {
                return nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());
            });

        if (hasOnlyCloudUrls)
            continue;

        const auto system =
            QnLocalSystemDescription::create(id.toSimpleString(), id, connection.systemName);

        static const int kVeryFarPriority = 100000;

        nx::vms::api::ModuleInformationWithAddresses fakeServerInfo;
        fakeServerInfo.id = QnUuid::createUuid();   //< It should be new unique id.
        fakeServerInfo.systemName = connection.systemName;
        fakeServerInfo.realm = QString::fromStdString(nx::network::AppInfo::realm());
        fakeServerInfo.cloudHost =
            QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost());

        // If version is not stored, default it to the current version.
        fakeServerInfo.version = !connection.version.isNull()
            ? connection.version
            : kSystemVersionWasNotStored;

        const auto searchAddresses = appContext()->coreSettings()->searchAddresses();
        const auto it = searchAddresses.find(id);
        if (it != searchAddresses.end())
        {
            for (const QSet<QString>& serverAddresses: it.value())
                fakeServerInfo.remoteAddresses += serverAddresses;
        }
        system->addServer(fakeServerInfo, kVeryFarPriority, false);
        system->setServerHost(fakeServerInfo.id, connection.urls.first());
        newSystems.insert(system->id(), system);
    }

    const auto newFinalSystems = filterOutSystems(newSystems);
    setFinalSystems(newFinalSystems);
}
