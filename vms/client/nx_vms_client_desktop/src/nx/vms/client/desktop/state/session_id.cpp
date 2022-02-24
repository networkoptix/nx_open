// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_id.h"

#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

QByteArray makeSessionId(const QString& systemId, const QString& userName)
{
    nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Sha256);
    hash.addData(systemId.toLower().toUtf8());
    hash.addData(userName.toLower().toUtf8());
    return hash.result();
}

} // namespace

SessionId::SessionId():
    SessionId(QString(), QString())
{
    // Actual values for the empty session id must correspond to empty system and user ids.
}

SessionId::SessionId(const QString& systemId, const QString& userId):
    m_bytes(makeSessionId(systemId, userId))
{
    NX_ASSERT(m_bytes.size() == SessionId::kDataSize);
}

SessionId SessionId::deserialized(const QByteArray& serializedValue)
{
    SessionId result;
    result.m_bytes = serializedValue;
    if (result.m_bytes.size() == SessionId::kDataSize)
        return result;

    return {};
}

QByteArray SessionId::serialized() const
{
    return m_bytes;
}

SessionId::operator QString() const
{
    return toString();
}

QString SessionId::toString() const
{
    return QString::fromLatin1(m_bytes.toBase64(QByteArray::Base64UrlEncoding));
}

SessionId SessionId::fromString(const QString& value)
{
    return SessionId::deserialized(
        QByteArray::fromBase64(value.toLatin1(), QByteArray::Base64UrlEncoding));
}

bool SessionId::operator!=(const SessionId& other) const
{
    return m_bytes != other.m_bytes;
}

bool SessionId::operator==(const SessionId& other) const
{
    return m_bytes == other.m_bytes;
}

void PrintTo(const SessionId& value, ::std::ostream* os)
{
    *os << value.toString().toStdString();
}

} // namespace nx::vms::client::desktop
