// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/settings/search_addresses_info.h>

namespace nx::vms::client::core {

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
        const nx::Uuid& localSystemId,
        const nx::Uuid& serverId,
        const QSet<QString>& remoteAddresses);

private:
    void update();

private:
    nx::vms::client::core::SystemSearchAddressesHash m_searchAddressesInfo;
};

} // namespace nx::vms::client::core
