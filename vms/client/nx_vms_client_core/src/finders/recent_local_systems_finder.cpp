// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recent_local_systems_finder.h"

#include <nx/network/app_info.h>
#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <network/local_system_description.h>

#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent): base_type(parent)
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
        {
            if (valueId == QnClientCoreSettings::RecentLocalConnections ||
                valueId == QnClientCoreSettings::SearchAddresses)
                updateSystems();
        });

    updateSystems();
}

void QnRecentLocalSystemsFinder::updateSystems()
{
    SystemsHash newSystems;
    const auto connections = qnClientCoreSettings->recentLocalConnections();
    for (auto it = connections.begin(); it != connections.end(); ++it)
    {
        if (it.key().isNull() || it->urls.isEmpty())
            continue;

        const auto connection = it.value();

        const bool hasOnlyCloudUrls = std::all_of(connection.urls.cbegin(), connection.urls.cend(),
            [](const nx::utils::Url& url)
            {
                return nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());
            });

        if (hasOnlyCloudUrls)
            continue;

        const auto system = QnLocalSystemDescription::create(
            it.key().toString(), it.key(), connection.systemName);

        static const int kVeryFarPriority = 100000;

        nx::vms::api::ModuleInformationWithAddresses fakeServerInfo;
        fakeServerInfo.id = QnUuid::createUuid();   // It SHOULD be new unique id
        fakeServerInfo.systemName = connection.systemName;
        fakeServerInfo.realm = QString::fromStdString(nx::network::AppInfo::realm());
        fakeServerInfo.cloudHost =
            QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost());
        const auto searchAddresses = qnClientCoreSettings->searchAddresses();
        const auto iter = searchAddresses.find(it.key());
        if (iter != searchAddresses.end())
        {
            for (const auto& serverAddressesList: searchAddresses[it.key()])
                fakeServerInfo.remoteAddresses += serverAddressesList;
        }
        system->addServer(fakeServerInfo, kVeryFarPriority, false);
        system->setServerHost(fakeServerInfo.id, connection.urls.first());
        newSystems.insert(system->id(), system);
    }

    const auto newFinalSystems = filterOutSystems(newSystems);
    setFinalSystems(newFinalSystems);
}
