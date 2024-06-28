// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_address_manager.h"

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {

SearchAddressManager::SearchAddressManager(QObject* parent):
    QObject(parent)
{
    connect(&appContext()->coreSettings()->recentLocalConnections,
        &nx::utils::property_storage::BaseProperty::changed,
        this,
        &SearchAddressManager::update);

    m_searchAddressesInfo = appContext()->coreSettings()->searchAddresses();

    update();
}

void SearchAddressManager::updateServerRemoteAddresses(
    const nx::Uuid& localSystemId,
    const nx::Uuid& serverId,
    const QSet<QString>& remoteAddresses)
{
    m_searchAddressesInfo[localSystemId][serverId] = remoteAddresses;

    const auto connections = appContext()->coreSettings()->recentLocalConnections();
    if (connections.contains(localSystemId))
    {
        auto searchAddressesInfo = appContext()->coreSettings()->searchAddresses();
        searchAddressesInfo[localSystemId][serverId] = remoteAddresses;
        appContext()->coreSettings()->searchAddresses = searchAddressesInfo;
    }
}

void SearchAddressManager::update()
{
    const auto connections = appContext()->coreSettings()->recentLocalConnections();

    auto searchAddresses = appContext()->coreSettings()->searchAddresses();

    for (const nx::Uuid& searchSystemId: searchAddresses.keys())
    {
        if (!connections.contains(searchSystemId))
            searchAddresses.remove(searchSystemId);
    }

    for (const auto& savedSystemId: connections.keys())
        searchAddresses[savedSystemId] = m_searchAddressesInfo[savedSystemId];

    appContext()->coreSettings()->searchAddresses = searchAddresses;
}

} // namespace nx::vms::client::core
