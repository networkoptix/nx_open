#include "encoded_string.h"

#include <nx/fusion/model_functions.h>

#include <utils/crypt/symmetrical.h>

namespace nx::vms::client::core {

EncodedString::EncodedString(const QByteArray& key):
    m_key(key)
{
}

EncodedString::EncodedString(const QString& value, const QByteArray& key):
    m_key(key),
    m_value(value)
{
}

EncodedString EncodedString::fromEncoded(const QString& encoded, const QByteArray& key)
{
    return EncodedString(nx::utils::decodeStringFromHexStringAES128CBC(encoded, key), key);
}

QString EncodedString::value() const
{
    return m_value;
}

void EncodedString::setValue(const QString& value)
{
    m_value = value;
}

QString EncodedString::encoded() const
{
    return nx::utils::encodeHexStringFromStringAES128CBC(m_value, m_key);
}

void EncodedString::setEncoded(const QString& encoded)
{
    m_value = nx::utils::decodeStringFromHexStringAES128CBC(encoded, m_key);
}

bool EncodedString::isEmpty() const
{
    return m_value.isEmpty();
}

bool EncodedString::operator==(const EncodedString& value) const
{
    return m_key == value.m_key && m_value == value.m_value;
}

bool EncodedString::operator!=(const EncodedString& value) const
{
    return !(*this == value);
}

QDebug operator<<(QDebug dbg, const EncodedString& value)
{
    dbg.nospace() << value.value() << " (" << value.encoded() << ")";
    return dbg.space();
}

QDataStream& operator<<(QDataStream& stream, const EncodedString& encodedString)
{
    return stream << encodedString.encoded();
}

QDataStream& operator>>(QDataStream& stream, EncodedString& encodedString)
{
    QString encoded;
    stream >> encoded;
    encodedString.setEncoded(encoded);
    return stream;
}

} // namespace nx::vms::client::core

void serialize(const nx::vms::client::core::EncodedString& value, QString* target)
{
    *target = value.encoded();
}

bool deserialize(const QString& value, nx::vms::client::core::EncodedString* target)
{
    *target = nx::vms::client::core::EncodedString::fromEncoded(value);
    return true;
}

void serialize(QnJsonContext *ctx, const nx::vms::client::core::EncodedString &value, QJsonValue *target)
{
    ::serialize(ctx, value.encoded(), target); /* Note the direct call instead of invocation through QJson. */
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, nx::vms::client::core::EncodedString *target)
{
    QString string;
    if (!::deserialize(ctx, value, &string)) /* Note the direct call instead of invocation through QJson. */
        return false;

    *target = nx::vms::client::core::EncodedString::fromEncoded(string);
    return true;
}
