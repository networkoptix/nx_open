// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/settings/search_addresses_info.h>

/**
 * Manages QnClientCoreSettings::SearchAddresses. Updates it when it's needed.
 * Should receive all found/updated servers remoteAddresses data.
 *
 * WARNING:
 * For current spec, there is no way to recognize deprecated servers for the particular system.
 * Thus, they would accumulate until system is removed from
 * QnClientCoreSettings::RecentLocalConnections.
 */
class SearchAddressManager: public QObject
{
    Q_OBJECT

public:
    SearchAddressManager(QObject* parent = nullptr);

    // Call in case of a server is found or updated.
    void updateServerRemoteAddresses(
        const QnUuid& localSystemId,
        const QnUuid& serverId,
        const QSet<QString>& remoteAddresses);

private:
    void update();

private:
    SystemSearchAddressesHash m_searchAddressesInfo;
};
