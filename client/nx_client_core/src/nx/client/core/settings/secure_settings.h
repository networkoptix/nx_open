#pragma once

#include <QtCore/QHash>

#include <nx/utils/property_storage/storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/fusion/serialization/json_functions.h>
#include <utils/common/encoded_credentials.h>

namespace nx::client::core {

class SecureSettings:
    public QObject,
    public nx::utils::property_storage::Storage,
    public Singleton<SecureSettings>
{
    Q_OBJECT

public:
    SecureSettings();

    using SystemAuthenticationDataHash = QHash<QnUuid, QList<QnEncodedCredentials>>;

    Property<SystemAuthenticationDataHash> systemAuthenticationData{
        this, "systemAuthenticationData"};
    Property<QnEncodedCredentials> cloudCredentials{
        this, "cloudCredentials"};
};

inline SecureSettings* secureSettings()
{
    return SecureSettings::instance();
}

} // namespace nx::client::core
