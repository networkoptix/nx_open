#pragma once

#include <QtCore/QHash>

#include <utils/common/credentials.h>

#include <nx/fusion/serialization/json_functions.h>

#include <nx/utils/property_storage/storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

class Settings:
    public nx::utils::property_storage::Storage,
    public Singleton<Settings>
{
    Q_OBJECT

public:
    Settings();

    using Credentials = nx::vms::common::Credentials;
    using SystemAuthenticationDataHash = QHash<QnUuid, QList<Credentials>>;

    // System credentials by local system id.
    SecureProperty<SystemAuthenticationDataHash> systemAuthenticationData{
        this, "secureSystemAuthenticationData"};

    SecureProperty<Credentials> cloudCredentials{
        this, "cloudCredentials"};

private:
    QByteArray m_securityKey;
};

inline Settings* settings()
{
    return Settings::instance();
}

} // namespace nx::vms::client::core
