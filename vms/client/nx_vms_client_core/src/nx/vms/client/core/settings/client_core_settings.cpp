// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_settings.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>

#include <utils/crypt/symmetrical.h>

#if defined(USE_QT_KEYCHAIN)
    #include <nx/vms/client/core/settings/keychain_property_storage_backend.h>
#endif

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/qsettings_backend.h>

#include <nx/branding.h>

template<>
nx::vms::client::core::Settings* Singleton<nx::vms::client::core::Settings>::s_instance = nullptr;

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

Settings::Settings(bool useKeyChain):
    Storage(new utils::property_storage::QSettingsBackend(createSettings(), kGroupName))
{
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
