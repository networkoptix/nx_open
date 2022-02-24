// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "search_address_manager.h"

#include <client_core/client_core_settings.h>

SearchAddressManager::SearchAddressManager(QObject* parent):
    QObject(parent)
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged,
        [this](int valueId)
        {
            if (valueId == QnClientCoreSettings::RecentLocalConnections)
                update();
        });

    m_searchAddressesInfo = qnClientCoreSettings->searchAddresses();

    update();
}

void SearchAddressManager::updateServerRemoteAddresses(
    const QnUuid& localSystemId,
    const QnUuid& serverId,
    const QSet<QString>& remoteAddresses)
{
    m_searchAddressesInfo[localSystemId][serverId] = remoteAddresses;

    const auto connections = qnClientCoreSettings->recentLocalConnections();
    if (connections.contains(localSystemId))
    {
        auto searchAddressesInfo = qnClientCoreSettings->searchAddresses();
        searchAddressesInfo[localSystemId][serverId] = remoteAddresses;
        qnClientCoreSettings->setSearchAddresses(searchAddressesInfo);
        qnClientCoreSettings->save();
    }
}

void SearchAddressManager::update()
{
    const auto connections = qnClientCoreSettings->recentLocalConnections();
    const QList<QnUuid> tmpSavedSystemIdsList =
        qnClientCoreSettings->recentLocalConnections().keys();
    const QSet<QnUuid> savedSystemIds(tmpSavedSystemIdsList.begin(), tmpSavedSystemIdsList.end());

    auto searchAddresses = qnClientCoreSettings->searchAddresses();
    const QList<QnUuid> searchSystemIds = searchAddresses.keys();

    bool hasChanges = false;
    for (const auto& searchSystemId: searchSystemIds)
    {
        if (!savedSystemIds.contains(searchSystemId))
        {
            searchAddresses.remove(searchSystemId);
            hasChanges = true;
        }
    }

    for (const auto& savedSystemId: savedSystemIds)
        searchAddresses[savedSystemId] = m_searchAddressesInfo[savedSystemId];

    qnClientCoreSettings->setSearchAddresses(searchAddresses);
    qnClientCoreSettings->save();
}
