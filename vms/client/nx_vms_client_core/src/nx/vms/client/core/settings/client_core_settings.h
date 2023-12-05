// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QtCore/QList>

#include <client_core/local_connection_data.h>
#include <network/cloud_system_data.h>
#include <nx/network/http/auth_tools.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/property_storage/storage.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/settings/search_addresses_info.h>
#include <nx/vms/client/core/settings/system_visibility_scope_info.h>
#include <nx/vms/client/core/system_logon/connection_data.h>
#include <nx/vms/client/core/watchers/known_server_connections.h>

#include "types/detected_object.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API Settings: public nx::utils::property_storage::Storage
{
    Q_OBJECT

public:
    struct InitializationOptions
    {
        /** Use QtKeychain library to store secure settings if possible. */
        bool useKeychain = true;

        /** Use QSettings backend to read old settings during the migration procedure. */
        bool useQSettingsBackend = false;

        /** An optional predefined securty key. Used for settings migration. */
        QByteArray securityKey;
    };

    Q_INVOKABLE static QVariant iniConfigValue(const QString& name);

    Settings(const InitializationOptions& options);
    virtual ~Settings() override;

    using SerializableCredentials = nx::network::http::SerializableCredentials;
    // Order is important as we are listing users by the last usage time.
    using SerializableCredentialsList = std::vector<SerializableCredentials>;
    using SystemAuthenticationData = std::unordered_map<QnUuid, SerializableCredentialsList>;

    // System credentials by local system id.
    SecureProperty<SystemAuthenticationData> systemAuthenticationData{
        this, "systemAuthenticationData"};


    SecureProperty<SerializableCredentials> cloudCredentials{
        this, "cloudCredentials"};

    struct PreferredCloudServer;
    using PreferredCloudServers = QList<PreferredCloudServer>;

    Property<PreferredCloudServers> preferredCloudServers{
        this, "PreferredCloudServers"};

    SecureProperty<ConnectionData> lastConnection{this, "lastConnection"};

    Property<DetectedObjectSettingsMap> detectedObjectSettings{this, "detectedObjectSettings"};

    // Password for cloud connections to the old systems. Needed for the compatibility.
    SecureProperty<std::string> digestCloudPassword{this, "digestCloudPassword"};

    using ValidationLevel = network::server_certificate::ValidationLevel;
    Property<ValidationLevel> certificateValidationLevel{
        this, "certificateValidationLevel", ValidationLevel::recommended};

    Property<QHash<QnUuid, LocalConnectionData>>
        recentLocalConnections{this, "recentLocalConnections"};

    Property<QList<WeightData>> localSystemWeightsData{this, "localSystemWeightsData"};

    Property<SystemSearchAddressesHash> searchAddresses{this, "searchAddresses"};

    Property<QList<watchers::KnownServerConnections::Connection>>
        knownServerConnections{this, "knownServerConnections"};

    Property<QSet<QString>> forgottenSystems{this, "forgottenSystems"};

    Property<QList<QnCloudSystem>> recentCloudSystems{this, "recentCloudSystems"};

    Property<welcome_screen::TileVisibilityScope> cloudTileScope{
        this, "cloudTileScope", welcome_screen::TileVisibilityScope::DefaultTileVisibilityScope};
    Property<SystemVisibilityScopeInfoHash> tileScopeInfo{this, "tileScopeInfo"};
    Property<welcome_screen::TileScopeFilter> tileVisibilityScopeFilter{
            this,
            "tileVisibilityScopeFilter",
            welcome_screen::TileScopeFilter::AllSystemsTileScopeFilter};

    Property<bool> enableHardwareDecoding{this, "enableHadrwareDecoding", true};

    /** Adapter for the cloudCredentials property. */
    CloudAuthData cloudAuthData() const;

    /** Adapter for the cloudCredentials property. */
    void setCloudAuthData(const CloudAuthData& value);

    /** Adapter for the preferredCloudServers property. */
    std::optional<QnUuid> preferredCloudServer(const QString& systemId);

    /** Adapter for the preferredCloudServers property. */
    void setPreferredCloudServer(const QString& systemId, const QnUuid& serverId);

    void storeRecentConnection(
        const QnUuid& localSystemId,
        const QString& systemName,
        const nx::utils::SoftwareVersion& version,
        const nx::utils::Url& url);

    void removeRecentConnection(const QnUuid& localSystemId);

    /** Update weight data as if client just logged in to the system. */
    void updateWeightData(const QnUuid& localId);

private:
    void migrateOldSettings();

    void migrateAllSettingsFrom_v51(Settings* oldSettings);

    // Migration from 4.2 to 5.0.
    void migrateSystemAuthenticationDataFrom_v42(Settings* oldSettings);

    void migrateWelcomeScreenSettingsFrom_v51(Settings* oldSettings);

    void clearInvalidKnownConnections();
};

struct Settings::PreferredCloudServer
{
    QString systemId;
    QnUuid serverId;
    bool operator==(const PreferredCloudServer& other) const = default;
};
NX_REFLECTION_INSTRUMENT(Settings::PreferredCloudServer, (systemId)(serverId))

} // namespace nx::vms::client::core
