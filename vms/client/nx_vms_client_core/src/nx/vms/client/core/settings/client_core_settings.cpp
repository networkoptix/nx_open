// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_settings.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>

#include <nx/branding.h>
#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/qsettings_backend.h>
#include <utils/crypt/symmetrical.h>

#if defined(USE_QT_KEYCHAIN)
    #include <nx/vms/client/core/settings/keychain_property_storage_backend.h>
#endif

namespace nx::vms::client::core {

namespace {

static const QString kGroupName("client_core");
static const QString kSecureKeyPropertyName("key");

QSettings* createSettings()
{
    return new QSettings(
        QSettings::NativeFormat,
        QSettings::UserScope,
        QCoreApplication::organizationName(),
        QCoreApplication::applicationName());
}

struct SerializableCredentials_v42
{
    std::string user;
    std::string password;

    operator nx::network::http::SerializableCredentials() const
    {
        nx::network::http::SerializableCredentials result;
        result.user = user;
        result.password = password;
        return result;
    }

    bool operator==(const SerializableCredentials_v42& other) const = default;
};
NX_REFLECTION_INSTRUMENT(SerializableCredentials_v42, (user)(password));

struct SystemAuthenticationData_v42
{
    QnUuid key;
    std::vector<SerializableCredentials_v42> value;
    bool operator==(const SystemAuthenticationData_v42& other) const = default;
};
NX_REFLECTION_INSTRUMENT(SystemAuthenticationData_v42, (key)(value));

} // namespace

static Settings* s_instance = nullptr;

Settings::Settings(bool useKeyChain):
    Storage(new utils::property_storage::QSettingsBackend(createSettings(), kGroupName))
{
    if (s_instance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_instance = this;

    if (useKeyChain)
    {
        #if defined(USE_QT_KEYCHAIN)
            const QString serviceName = nx::branding::company() + " " + nx::branding::vmsName();
            KeychainBackend keychain(serviceName);

            QByteArray key = keychain.readValue(kSecureKeyPropertyName).toUtf8();
            if (key.isEmpty())
            {
                key = nx::utils::generateAesExtraKey();
                if (!keychain.writeValue(kSecureKeyPropertyName, QString::fromUtf8(key)))
                {
                    NX_WARNING(this, "Keychain is not available, using predefined key");
                    key = {};
                }
            }

            setSecurityKey(key);
        #endif
    }

    load();
    migrateSystemAuthenticationData();
}

Settings::~Settings()
{
    if (s_instance == this)
        s_instance = nullptr;
}

Settings* Settings::instance()
{
    return s_instance;
}

CloudAuthData Settings::cloudAuthData() const
{
    nx::network::http::Credentials credentials = cloudCredentials();

    CloudAuthData authData;
    authData.credentials.authToken.type = nx::network::http::AuthTokenType::bearer;
    authData.credentials.username = std::move(credentials.username);
    authData.refreshToken = std::move(credentials.authToken.value);
    return authData;
}

void Settings::setCloudAuthData(const CloudAuthData& authData)
{
    // Save username so it can be restored for future connections to cloud.
    const std::string username = authData.credentials.username.empty()
        ? settings()->cloudCredentials().user
        : authData.credentials.username;

    cloudCredentials = nx::network::http::Credentials(
        username,
        nx::network::http::BearerAuthToken(authData.refreshToken));
}

std::optional<QnUuid> Settings::preferredCloudServer(const QString& systemId)
{
    const auto preferredServers = preferredCloudServers();
    const auto iter = std::find_if(preferredServers.cbegin(), preferredServers.cend(),
        [&systemId](const auto& item) { return item.systemId == systemId; });

    if (iter == preferredServers.cend())
        return std::nullopt;

    return iter->serverId;
}

void Settings::setPreferredCloudServer(const QString& systemId, const QnUuid& serverId)
{
    static constexpr int kMaxStoredPreferredCloudServers = 100;

    auto preferredServers = preferredCloudServers();
    const auto iter = std::find_if(preferredServers.begin(), preferredServers.end(),
        [&systemId](const auto& item) { return item.systemId == systemId; });

    if (iter != preferredServers.end())
        preferredServers.erase(iter);

    NX_DEBUG(this, "Setting server %1 as preferred for the cloud system %2",
        serverId.toSimpleString(), systemId);

    preferredServers.push_back({systemId, serverId});

    while (preferredServers.size() > kMaxStoredPreferredCloudServers)
        preferredServers.pop_front();

    preferredCloudServers = preferredServers;
}

void Settings::migrateSystemAuthenticationData()
{
    // Check if data is already migrated.
    SystemAuthenticationData migratedData = systemAuthenticationData();
    if (!migratedData.empty())
        return;

    SecureProperty<std::vector<SystemAuthenticationData_v42>> systemAuthenticationData_v42{
        this, "secureSystemAuthenticationData"};

    loadProperty(&systemAuthenticationData_v42);

    const auto oldData = systemAuthenticationData_v42();
    for (const SystemAuthenticationData_v42& authData: oldData)
    {
        const QnUuid& localSystemId = authData.key;
        for (const SerializableCredentials_v42& credentials: authData.value)
            migratedData[localSystemId].push_back(credentials);
    }

    systemAuthenticationData = migratedData;
    systemAuthenticationData_v42 = {};
}

} // namespace nx::vms::client::core
