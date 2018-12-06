#pragma once

#include <QtCore/QHash>

#include <nx/vms/client/core/common/utils/encoded_credentials.h>

#include <nx/utils/property_storage/storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/fusion/serialization/json_functions.h>

namespace nx::vms::client::core {

class SecureSettings:
    public QObject,
    public nx::utils::property_storage::Storage,
    public Singleton<SecureSettings>
{
    Q_OBJECT

public:
    SecureSettings();

    using SystemAuthenticationDataHash = QHash<QnUuid, QList<EncodedCredentials>>;

    // System credentials by local system id.
    Property<SystemAuthenticationDataHash> systemAuthenticationData{
        this, "systemAuthenticationData"};

    Property<EncodedCredentials> cloudCredentials{
        this, "cloudCredentials"};
};

inline SecureSettings* secureSettings()
{
    return SecureSettings::instance();
}

} // namespace nx::vms::client::core
