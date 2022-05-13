// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <vector>

#include <QtCore/QList>

#include <nx/network/http/auth_tools.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/property_storage/storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/system_logon/connection_data.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API Settings:
    public nx::utils::property_storage::Storage,
    public Singleton<Settings>
{
    Q_OBJECT

public:
    /** Use QtKeyChain library to store secure settings if possible. */
    Settings(bool useKeyChain = true);

    using SerializableCredentials = nx::network::http::SerializableCredentials;
    // Order is important as we are listing users by the last usage time.
    using SerializableCredentialsList = std::vector<SerializableCredentials>;
    using SystemAuthenticationData = std::unordered_map<QnUuid, SerializableCredentialsList>;

    // System credentials by local system id.
    SecureProperty<SystemAuthenticationData> systemAuthenticationData{
        this, "secureSystemAuthenticationData_v50"};

    /** Password cloud credentials for versions 4.2 and below. */
    SecureProperty<SerializableCredentials> cloudPasswordCredentials{
        this, "cloudCredentials"};

    SecureProperty<SerializableCredentials> cloudCredentials{
        this, "cloudCredentials_v50"};

    struct PreferredCloudServer;
    using PreferredCloudServers = QList<PreferredCloudServer>;

    Property<PreferredCloudServers> preferredCloudServers{
        this, "PreferredCloudServers"};

    SecureProperty<ConnectionData> lastConnection{this, "LastConnection"};

    // Password for cloud connections to the old systems. Needed for the compatibility.
    SecureProperty<std::string> digestCloudPassword{this, "DigestCloudPassword"};

    using ValidationLevel = network::server_certificate::ValidationLevel;
    Property<ValidationLevel> certificateValidationLevel{
        this, "CertificateValidationLevel", ValidationLevel::recommended};

    Property<bool> enableHardwareDecoding{this, "EnableHadrwareDecoding", true};

private:
    // Migration from 4.2 to 4.3.
    void migrateSystemAuthenticationData();

private:
    QByteArray m_securityKey;
};

inline Settings* settings()
{
    return Settings::instance();
}

struct Settings::PreferredCloudServer
{
    QString systemId;
    QnUuid serverId;
    bool operator==(const PreferredCloudServer& other) const = default;
};
NX_REFLECTION_INSTRUMENT(Settings::PreferredCloudServer, (systemId)(serverId));

} // namespace nx::vms::client::core
