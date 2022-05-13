// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utils/common/property_storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/url.h>
#include <client_core/local_connection_data.h>
#include <client/forgotten_systems_manager.h>
#include <watchers/cloud_status_watcher.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>
#include <nx/vms/client/core/settings/welcome_screen_info.h>
#include <nx/vms/client/core/settings/system_visibility_scope_info.h>
#include <nx/vms/client/core/settings/search_addresses_info.h>

class QSettings;

class NX_VMS_CLIENT_CORE_API QnClientCoreSettings :
    public QnPropertyStorage,
    public Singleton<QnClientCoreSettings>
{
    Q_OBJECT
    using base_type = QnPropertyStorage;

public:
    enum PropertyIdentifier
    {
        RecentLocalConnections,
        SearchAddresses,
        LocalSystemWeightsData,
        RecentCloudSystems,
        ForgottenSystems,
        KnownServerConnections,

        // Welcome screen settings.
        CloudTileScope,
        TileScopeInfo,
        TileVisibilityScopeFilter,

        PropertiesCount
    };

    using RecentLocalConnectionsHash = QHash<QnUuid, nx::vms::client::core::LocalConnectionData>;
    using WeightDataList = QList<nx::vms::client::core::WeightData>;
    using KnownServerConnection = nx::vms::client::core::watchers::KnownServerConnections::Connection;
    using TileVisibilityScope = nx::vms::client::core::welcome_screen::TileVisibilityScope;
    using TileScopeFilter = nx::vms::client::core::welcome_screen::TileScopeFilter;

public:
    QnClientCoreSettings(QObject* parent = nullptr);

    virtual ~QnClientCoreSettings();

    virtual void writeValueToSettings(
            QSettings* settings,
            int id,
            const QVariant& value) const override;

    virtual QVariant readValueFromSettings(
            QSettings* settings,
            int id,
            const QVariant& defaultValue) const override;

    void save();

private:
    QN_BEGIN_PROPERTY_STORAGE(PropertiesCount)
        QN_DECLARE_RW_PROPERTY(RecentLocalConnectionsHash,
            recentLocalConnections, setRecentLocalConnections,
            RecentLocalConnections, RecentLocalConnectionsHash())
        QN_DECLARE_RW_PROPERTY(SystemSearchAddressesHash,
            searchAddresses, setSearchAddresses,
            SearchAddresses, SystemSearchAddressesHash())
        QN_DECLARE_RW_PROPERTY(WeightDataList,
            localSystemWeightsData, setLocalSystemWeightsData,
            LocalSystemWeightsData, WeightDataList())
        QN_DECLARE_RW_PROPERTY(QnCloudSystemList,
            recentCloudSystems, setRecentCloudSystems,
            RecentCloudSystems, QnCloudSystemList())
        QN_DECLARE_RW_PROPERTY(int,
            cloudTileScope, setCloudTileScope,
            CloudTileScope, TileVisibilityScope::DefaultTileVisibilityScope)
        QN_DECLARE_RW_PROPERTY(SystemVisibilityScopeInfoHash,
            tileScopeInfo, setTileScopeInfo,
            TileScopeInfo, SystemVisibilityScopeInfoHash())
        QN_DECLARE_RW_PROPERTY(int,
            tileVisibilityScopeFilter, setTileVisibilityScopeFilter,
            TileVisibilityScopeFilter, TileScopeFilter::AllSystemsTileScopeFilter)

        QN_DECLARE_RW_PROPERTY(QnStringSet,
            forgottenSystems, setForgottenSystems,
            ForgottenSystems, QnStringSet())
        QN_DECLARE_RW_PROPERTY(QList<KnownServerConnection>,
            knownServerConnections, setKnownServerConnections,
            KnownServerConnections, QList<KnownServerConnection>())
    QN_END_PROPERTY_STORAGE()

private:
    QSettings* m_settings;
};

#define qnClientCoreSettings QnClientCoreSettings::instance()
