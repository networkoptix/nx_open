#include "encoded_string.h"

#include <utils/crypt/symmetrical.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

using namespace nx::utils;

EncodedString::EncodedString(const QByteArray& key):
    m_key(key)
{
}

EncodedString EncodedString::fromDecoded(const QString& decoded, const QByteArray& key)
{
    EncodedString result(key);
    result.setDecoded(decoded);
    NX_ASSERT(result.m_mode == Mode::decoded);
    return result;
}

EncodedString EncodedString::fromEncoded(const QString& encoded, const QByteArray& key)
{
    EncodedString result(key);
    result.setEncoded(encoded);
    NX_ASSERT(result.m_mode == Mode::encoded);
    return result;
}

QByteArray EncodedString::key() const
{
    return m_key;
}

void EncodedString::setKey(const QByteArray& key)
{
    if (m_key == key)
        return;

    m_key = key;

    // Invalidate cached value.
    switch (m_mode)
    {
        case Mode::empty:
            break;

        case Mode::encoded:
            m_decodedValue = std::nullopt;
            break;

        case Mode::decoded:
            m_encodedValue = std::nullopt;
            break;
    }
}

QString EncodedString::decoded() const
{
    switch (m_mode)
    {
        case Mode::empty:
            return QString();

        case Mode::encoded:
            NX_ASSERT(m_encodedValue);
            if (!m_decodedValue)
                m_decodedValue = decodeStringFromHexStringAES128CBC(*m_encodedValue, m_key);
            return *m_decodedValue;

        case Mode::decoded:
            NX_ASSERT(m_decodedValue);
            return *m_decodedValue;
    }

    NX_ASSERT("Unreachable code");
    return QString();
}

void EncodedString::setDecoded(const QString& value)
{
    NX_ASSERT(m_mode != Mode::encoded);
    m_mode = Mode::decoded;
    m_decodedValue = value;
    m_encodedValue = std::nullopt;
}

QString EncodedString::encoded() const
{
    switch (m_mode)
    {
        case Mode::empty:
            return QString();

        case Mode::encoded:
            NX_ASSERT(m_encodedValue);
            return *m_encodedValue;

        case Mode::decoded:
            NX_ASSERT(m_decodedValue);
            if (!m_encodedValue)
                m_encodedValue = encodeHexStringFromStringAES128CBC(*m_decodedValue, m_key);
            return *m_encodedValue;
    }

    NX_ASSERT("Unreachable code");
    return QString();
}

void EncodedString::setEncoded(const QString& encoded)
{
    NX_ASSERT(m_mode != Mode::decoded);
    m_mode = Mode::encoded;
    m_encodedValue = encoded;
    m_decodedValue = std::nullopt;
}

bool EncodedString::isEmpty() const
{
    return m_mode == Mode::empty;
}

bool EncodedString::operator==(const EncodedString& value) const
{
    if (m_key != value.m_key)
        return false;

    switch (m_mode)
    {
        case Mode::empty:
            return true;

        case Mode::encoded:
            return m_encodedValue == value.encoded();

        case Mode::decoded:
            return m_decodedValue == value.decoded();
    }

    NX_ASSERT("Unreachable code");
    return false;
}

bool EncodedString::operator!=(const EncodedString& value) const
{
    return !(*this == value);
}

QDebug operator<<(QDebug dbg, const EncodedString& value)
{
    dbg.nospace() << value.decoded() << " (" << value.encoded() << ")";
    return dbg.space();
}

QDataStream& operator<<(QDataStream& stream, const EncodedString& value)
{
    return stream << value.encoded();
}

QDataStream& operator>>(QDataStream& stream, EncodedString& value)
{
    QString encoded;
    stream >> encoded;
    value.setEncoded(encoded);
    return stream;
}

void PrintTo(const EncodedString& value, ::std::ostream* os)
{
    *os << value.decoded().toStdString()
        << " ("
        << value.key().toBase64().toStdString()
        << "@"
        << value.encoded().toStdString()
        << ")";
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

void serialize(
    QnJsonContext* ctx,
    const nx::vms::client::core::EncodedString& value,
    QJsonValue* target)
{
    // Note the direct call instead of invocation through QJson.
    ::serialize(ctx, value.encoded(), target);
}

bool deserialize(
    QnJsonContext* ctx,
    const QJsonValue& value,
    nx::vms::client::core::EncodedString* target)
{
    QString string;
    // Note the direct call instead of invocation through QJson.
    if (!::deserialize(ctx, value, &string))
        return false;

    *target = nx::vms::client::core::EncodedString::fromEncoded(string);
    return true;
}
