#pragma once

#include <QtCore/QHash>

#include <nx/vms/client/core/common/utils/encoded_string.h>

#include <utils/common/credentials.h>

#include <nx/fusion/serialization/json_functions.h>

#include <nx/utils/property_storage/storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

class Secured
{
public:
    QByteArray securityKey() const
    {
        return m_securityKey;
    }

    void setSecurityKey(const QByteArray& key)
    {
        m_securityKey = key;
    }

private:
    QByteArray m_securityKey;
};

template<typename T>
class SecureProperty: public nx::utils::property_storage::Property<T>, public Secured
{
    using base_type = nx::utils::property_storage::Property<T>;
public:
    using base_type::Property;
    using base_type::operator=;

protected:
    virtual T decodeValue(const QString& value, bool* success) const override
    {
        const auto converter = EncodedString::fromEncoded(value, securityKey());
        return base_type::decodeValue(converter.decoded(), success);
    }

    virtual QString encodeValue(const T& value) const override
    {
        const auto converter = EncodedString::fromDecoded(
            base_type::encodeValue(value), securityKey());
        return converter.encoded();
    }
};

class Settings:
    public QObject,
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
};

inline Settings* settings()
{
    return Settings::instance();
}

} // namespace nx::vms::client::core
