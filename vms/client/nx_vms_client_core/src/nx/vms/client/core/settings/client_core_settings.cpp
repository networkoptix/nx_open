// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_settings.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>

#include <nx/branding.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/app_info.h>
#include <nx/utils/crypt/symmetrical.h>
#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/filesystem_backend.h>
#include <nx/utils/property_storage/qsettings_backend.h>
#include <nx/utils/system_utils.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/settings/ini_helpers.h>

#if defined(USE_QT_KEYCHAIN)
    #include <nx/vms/client/core/settings/keychain_property_storage_backend.h>
#endif

using namespace std::chrono;

namespace nx::vms::client::core {

namespace {

static const QString kSecureKeyPropertyName("key");

nx::utils::property_storage::AbstractBackend* createBackend(bool useQSettings)
{
    if (useQSettings)
        return new nx::utils::property_storage::QSettingsBackend(new QSettings(), "client_core");

    return new nx::utils::property_storage::FileSystemBackend(
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/client_core");
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LocalConnectionData, (json), (systemName)(urls))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemVisibilityScopeInfo, (json), (name)(visibilityScope))

QVariant Settings::iniConfigValue(const QString& name)
{
    return getIniValue(ini(), name);
}

Settings::Settings(const InitializationOptions& options):
    Storage(createBackend(options.useQSettingsBackend))
{
    qRegisterMetaType<QSet<QString>>("QnStringSet");

    if (options.useKeychain)
    {
        #if defined(USE_QT_KEYCHAIN)
            const QString serviceName = nx::branding::company() + " " + nx::branding::vmsName();
            KeychainBackend keychain(serviceName);

            QByteArray key = keychain.readValue(kSecureKeyPropertyName).toUtf8();
            if (key.isEmpty())
            {
                key = nx::crypt::generateAesExtraKey();
                if (!keychain.writeValue(kSecureKeyPropertyName, QString::fromUtf8(key)))
                {
                    NX_WARNING(this, "Keychain is not available, using predefined key");
                    key = {};
                }
            }

            setSecurityKey(key);
        #endif
    }
    else
    {
        setSecurityKey(options.securityKey);
    }

    if (options.useQSettingsBackend)
        setReadOnly(true);

    load();

    if (!options.useQSettingsBackend)
    {
        migrateOldSettings();
        clearInvalidKnownConnections();
    }
}

Settings::~Settings()
{
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
        ? cloudCredentials().user
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

void Settings::storeRecentConnection(
    const QnUuid& localSystemId,
    const QString& systemName,
    const nx::utils::SoftwareVersion& version,
    const nx::utils::Url& url)
{
    NX_VERBOSE(NX_SCOPE_TAG, "Storing recent system connection id: %1, url: %2",
        localSystemId, url);

    const auto cleanUrl = url.cleanUrl();

    auto connections = recentLocalConnections();

    auto& data = connections[localSystemId];
    data.systemName = systemName;
    data.version = version;
    data.urls.removeOne(cleanUrl);
    data.urls.prepend(cleanUrl);

    recentLocalConnections = connections;
}

void Settings::removeRecentConnection(const QnUuid& localSystemId)
{
    NX_VERBOSE(NX_SCOPE_TAG, "Removing recent system connection id: %1", localSystemId);

    auto connections = recentLocalConnections();
    connections.remove(localSystemId);
    recentLocalConnections = connections;
}

void Settings::updateWeightData(const QnUuid& localId)
{
    auto weightData = localSystemWeightsData();
    const auto itWeightData = std::find_if(weightData.begin(), weightData.end(),
        [localId](const WeightData& data) { return data.localId == localId; });

    auto currentWeightData = (itWeightData == weightData.end()
        ? WeightData{localId, 0, QDateTime::currentMSecsSinceEpoch(), true}
        : *itWeightData);

    currentWeightData.weight = nx::utils::calculateSystemUsageFrequency(
        time_point<system_clock>(milliseconds(currentWeightData.lastConnectedUtcMs)),
        currentWeightData.weight) + 1;
    currentWeightData.lastConnectedUtcMs = QDateTime::currentMSecsSinceEpoch();
    currentWeightData.realConnection = true;

    if (itWeightData == weightData.end())
        weightData.append(currentWeightData);
    else
        *itWeightData = currentWeightData;

    localSystemWeightsData = weightData;
}

void Settings::migrateOldSettings()
{
    auto oldSettings = std::make_unique<Settings>(
        InitializationOptions{
            .useKeychain = false,
            .useQSettingsBackend = true,
            .securityKey = securityKey()});

    migrateAllSettingsFrom_v51(oldSettings.get());
    migrateSystemAuthenticationDataFrom_v42(oldSettings.get());
}

void Settings::migrateAllSettingsFrom_v51(Settings* oldSettings)
{
    using namespace utils::property_storage;

    AbstractBackend* backend = this->backend();
    AbstractBackend* oldBackend = oldSettings->backend();

    const QHash<QString, QString> oldNames{
        {"systemAuthenticationData", "secureSystemAuthenticationData_v50"},
        {"preferredCloudServers", "PreferredCloudServers"},
        {"cloudCredentials", "cloudCredentials_v50"},
        {"lastConnection", "LastConnection"},
        {"digestCloudPassword", "DigestCloudPassword"},
        {"certificateValidationLevel", "CertificateValidationLevel"},
        {"enableHadrwareDecoding", "EnableHadrwareDecoding"},
    };

    // Will be processed another way.
    const QSet<QString> propertiesToSkip{
        forgottenSystems.name,
        recentLocalConnections.name,
        searchAddresses.name,
        tileScopeInfo.name,
    };

    // Do a low-level transfer of properties.
    for (BaseProperty* property: properties())
    {
        if (property->exists())
            continue;

        if (propertiesToSkip.contains(property->name))
            continue;

        const QString oldName = oldNames.value(property->name, property->name);

        if (oldBackend->exists(oldName))
            backend->writeValue(property->name, oldBackend->readValue(oldName));
    }

    // Since we used backend functions to transfer the properties, we need to re-read them.
    load();

    migrateWelcomeScreenSettingsFrom_v51(oldSettings);
}

void Settings::migrateSystemAuthenticationDataFrom_v42(Settings* oldSettings)
{
    // Check if data is already migrated.
    SystemAuthenticationData migratedData = systemAuthenticationData();
    if (!migratedData.empty())
        return;

    SecureProperty<std::vector<SystemAuthenticationData_v42>> systemAuthenticationData_v42{
        oldSettings, "secureSystemAuthenticationData"};

    oldSettings->loadProperty(&systemAuthenticationData_v42);

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

void Settings::migrateWelcomeScreenSettingsFrom_v51(Settings* oldSettings)
{
    static constexpr nx::utils::SoftwareVersion k51Version{5, 1, 0, 0};
    using namespace utils::property_storage;

    const QString kClientCorePrefix = "client_core/";

    AbstractBackend* oldBackend = oldSettings->backend();
    auto oldQSettings = static_cast<QSettingsBackend*>(oldBackend)->qSettings();

    if (!forgottenSystems.exists())
    {
        const auto oldForgottenSystems =
            oldQSettings->value(kClientCorePrefix + forgottenSystems.name).value<QSet<QString>>();
        forgottenSystems = {oldForgottenSystems.begin(), oldForgottenSystems.end()};
    }

    // nx_fusion has different format of associative containers serialization. Use it to read old
    // values.
    if (!recentLocalConnections.exists())
    {
        QHash<QnUuid, LocalConnectionData> value;
        if (QJson::deserialize(oldQSettings->value(
            kClientCorePrefix + recentLocalConnections.name).toString(), &value))
        {
            // When migrating, assume all systems were still 5.1.
            for (auto it = value.begin(); it != value.end(); ++it)
                it->version = k51Version;

            recentLocalConnections = value;
        }
    }

    if (!searchAddresses.exists())
    {
        SystemSearchAddressesHash oldSearchAddresses;
        if (QJson::deserialize(oldQSettings->value(
            kClientCorePrefix + searchAddresses.name).toString(), &oldSearchAddresses))
        {
            searchAddresses = oldSearchAddresses;
        }
    }

    if (!tileScopeInfo.exists())
    {
        SystemVisibilityScopeInfoHash oldScopeInfo;
        if (QJson::deserialize(oldQSettings->value(
            kClientCorePrefix + tileScopeInfo.name).toString(), &oldScopeInfo))
        {
            tileScopeInfo = oldScopeInfo;
        }
    }
}

void Settings::clearInvalidKnownConnections()
{
    auto connections = knownServerConnections();

    for (auto it = connections.begin(); it != connections.end(); /*no increment*/)
    {
        if (!it->url.isValid() || it->url.host().isEmpty())
            it = connections.erase(it);
        else
            ++it;
    }

    knownServerConnections = connections;
}

} // namespace nx::vms::client::core
