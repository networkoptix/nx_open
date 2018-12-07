#pragma once

#include <QtCore/QHash>

#include <utils/common/credentials.h>

#include <nx/fusion/serialization/json_functions.h>

#include <nx/utils/property_storage/storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

class Settings:
    public QObject,
    public nx::utils::property_storage::Storage,
    public Singleton<Settings>
{
    Q_OBJECT

public:
    using Credentials = nx::vms::common::Credentials;

    Settings();

    using SystemAuthenticationDataHash = QHash<QnUuid, QList<Credentials>>;

    // System credentials by local system id.
    Property<SystemAuthenticationDataHash> systemAuthenticationData{
        this, "secureSystemAuthenticationData"};

    Property<Credentials> cloudCredentials{
        this, "cloudCredentials"};

private:
    QByteArray m_securityKey;
};

inline Settings* settings()
{
    return Settings::instance();
}

} // namespace nx::vms::client::core
