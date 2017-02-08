#include "encoded_string.h"

#include <nx/fusion/model_functions.h>

#include <utils/crypt/symmetrical.h>

QnEncodedString::QnEncodedString()
{
}

QnEncodedString::QnEncodedString(const QString& value):
    m_value(value)
{
}

QnEncodedString QnEncodedString::fromEncoded(const QString& encoded)
{
    return QnEncodedString(nx::utils::decodeStringFromHexStringAES128CBC(encoded));
}

QString QnEncodedString::value() const
{
    return m_value;
}

void QnEncodedString::setValue(const QString& value)
{
    m_value = value;
}

QString QnEncodedString::encoded() const
{
    return nx::utils::encodeHexStringFromStringAES128CBC(m_value);
}

void QnEncodedString::setEncoded(const QString& encoded)
{
    m_value = nx::utils::decodeStringFromHexStringAES128CBC(encoded);
}

bool QnEncodedString::isEmpty() const
{
    return m_value.isEmpty();
}

bool QnEncodedString::operator==(const QnEncodedString& value) const
{
    return m_value == value.m_value;
}

bool QnEncodedString::operator!=(const QnEncodedString& value) const
{
    return m_value != value.m_value;
}

void serialize(const QnEncodedString& value, QString* target)
{
    *target = value.encoded();
}

bool deserialize(const QString& value, QnEncodedString* target)
{
    *target = QnEncodedString::fromEncoded(value);
    return true;
}

void serialize(QnJsonContext *ctx, const QnEncodedString &value, QJsonValue *target)
{
    ::serialize(ctx, value.encoded(), target); /* Note the direct call instead of invocation through QJson. */
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnEncodedString *target)
{
    QString string;
    if (!::deserialize(ctx, value, &string)) /* Note the direct call instead of invocation through QJson. */
        return false;

    *target = QnEncodedString::fromEncoded(string);
    return true;
}

QDebug operator<<(QDebug dbg, const QnEncodedString& value)
{
    dbg.nospace() << value.value() << " (" << value.encoded() << ")";
    return dbg.space();
}

QDataStream& operator<<(QDataStream& stream, const QnEncodedString& encodedString)
{
    return stream << encodedString.encoded();
}

QDataStream& operator>>(QDataStream& stream, QnEncodedString& encodedString)
{
    QString encoded;
    stream >> encoded;
    encodedString.setEncoded(encoded);
    return stream;
}
