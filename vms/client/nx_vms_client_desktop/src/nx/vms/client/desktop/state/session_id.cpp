// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_id.h"

#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

namespace {

QByteArray makeSessionId(const QString& systemId, const QString& userName)
{
    nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Sha256);
    hash.addData(userName.toLower().toUtf8());
    hash.addData(systemId.toLower().toUtf8());
    return hash.result();
}

const QByteArray kEmptySessionId = makeSessionId(QString(), QString());

} // namespace

SessionId::SessionId():
    m_bytes(kEmptySessionId)
{
    // Actual values for the empty session id must correspond to empty system and user ids.
}

SessionId::SessionId(
    const nx::Uuid& localSystemId,
    const QString& systemId,
    const QString& userId)
    :
    m_bytes(makeSessionId(systemId, userId)),
    m_localSystemId(localSystemId),
    m_systemId(systemId),
    m_userId(userId)
{
    NX_ASSERT(!localSystemId.isNull());
    NX_ASSERT(!m_systemId.isNull());
    NX_ASSERT(!m_userId.isNull());
    NX_ASSERT(m_bytes.size() == SessionId::kDataSize);
}

bool SessionId::isEmpty() const
{
    return m_bytes == kEmptySessionId;
}

SessionId::operator bool() const
{
    return !isEmpty();
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

std::string SessionId::toString() const
{
    return m_bytes.toBase64(QByteArray::Base64UrlEncoding).toStdString();
}

QString SessionId::toQString() const
{
    return QString::fromStdString(toString());
}

QString SessionId::legacyIdString() const
{
    return QString::fromLatin1(
        makeSessionId(m_localSystemId.toString(), m_userId)
            .toBase64(QByteArray::Base64UrlEncoding));
}

SessionId SessionId::fromString(const std::string& value)
{
    return SessionId::deserialized(
        QByteArray::fromBase64(QByteArray::fromStdString(value), QByteArray::Base64UrlEncoding));
}

QString SessionId::toLogString() const
{
    if (!m_userId.isEmpty() || !m_systemId.isEmpty())
        return nx::format("%1:%2(%3) [%4]", m_userId, m_systemId, m_localSystemId, toString());

    return QString::fromStdString(toString());
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
    *os << value.toLogString().toStdString();
}

} // namespace nx::vms::client::desktop
