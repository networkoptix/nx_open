// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uuid.h"

#include <type_traits>

#include <QtCore/QCryptographicHash>

#include <nx/utils/log/assert.h>

namespace nx {

Uuid::Uuid(const char* text)
{
    const auto& qByteArrayText =
        text ? QByteArray::fromRawData(text, static_cast<int>(strlen(text))) : QByteArray();
    NX_ASSERT(isUuidString(qByteArrayText), text);
    m_uuid = QUuid(qByteArrayText);
}

Uuid::Uuid(const QString& text): m_uuid(text)
{
    NX_ASSERT(isUuidString(text), text);
}

Uuid::Uuid(const QByteArray& text): m_uuid(text)
{
    NX_ASSERT(isUuidString(text), text);
}

Uuid::Uuid(const std::string& text):
    m_uuid(QByteArray::fromStdString(text))
{
    NX_ASSERT(isUuidString(text), text);
}

Uuid::Uuid(const QUuid& uuid): m_uuid(uuid)
{
}

bool Uuid::isUuidString(const QByteArray& data)
{
    return data.isEmpty()
        || data.size() == 36 //< xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        || data.size() == 38; //< {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
}

bool Uuid::isUuidString(const QString& data)
{
    return data.isEmpty()
        || data.size() == 36 //< xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        || data.size() == 38; //< {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
}

bool Uuid::isUuidString(const std::string& data)
{
    return data.empty()
        || data.size() == 36 //< xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        || data.size() == 38; //< {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
}

const QUuid& Uuid::getQUuid() const
{
    return m_uuid;
}

bool Uuid::isNull() const
{
    return m_uuid.isNull();
}

const QByteArray Uuid::toByteArray() const
{
    return m_uuid.toByteArray();
}

const QByteArray Uuid::toRfc4122() const
{
    return m_uuid.toRfc4122();
}

const QString Uuid::toString() const
{
    return m_uuid.toString();
}

QString Uuid::toSimpleString() const
{
    return m_uuid.toString(QUuid::WithoutBraces);
}

QByteArray Uuid::toSimpleByteArray() const
{
    return m_uuid.toByteArray(QUuid::WithoutBraces);
}

std::string Uuid::toStdString() const
{
    const auto& byteArray = toByteArray();
    return std::string(byteArray.constData(), byteArray.size());
}

std::string Uuid::toSimpleStdString() const
{
    return toSimpleByteArray().toStdString();
}

QUuid Uuid::toQUuid() const
{
    return m_uuid;
}

bool Uuid::operator<(const Uuid& other) const
{
    return m_uuid < other.m_uuid;
}

bool Uuid::operator>(const Uuid& other) const
{
    return m_uuid > other.m_uuid;
}

Uuid Uuid::fromRfc4122(const QByteArray& bytes)
{
    Uuid _uuid;
    _uuid.m_uuid = QUuid::fromRfc4122(bytes);
    return _uuid;
}

Uuid Uuid::fromHardwareId(const QString& hwid)
{
    if (hwid.length() != 34)
        return Uuid();

    return Uuid(QString::fromLatin1("%1-%2-%3-%4-%5")
        .arg(hwid.mid(2, 8)).arg(hwid.mid(10, 4)).arg(hwid.mid(14, 4))
        .arg(hwid.mid(18, 4)).arg(hwid.mid(22, 12)));
}

Uuid Uuid::createUuid()
{
    Uuid _uuid;
    _uuid.m_uuid = QUuid::createUuid();
    return _uuid;
}

Uuid Uuid::fromStringSafe(const QString& uuid)
{
    return Uuid(QUuid(uuid));
}

Uuid Uuid::fromStringSafe(const QByteArray& uuid)
{
    return Uuid(QUuid(uuid));
}

Uuid Uuid::fromStringSafe(const char* uuid)
{
    return Uuid(QUuid(uuid));
}

Uuid Uuid::fromStringSafe(const std::string_view& uuid)
{
    return fromStringSafe(QByteArray::fromRawData(uuid.data(), (int) uuid.size()));
}

Uuid Uuid::fromArbitraryData(const QByteArray& data)
{
    QCryptographicHash md5Hash(QCryptographicHash::Md5);
    md5Hash.addData(data);
    QByteArray bytes = md5Hash.result();
    return fromRfc4122(bytes);
}

Uuid Uuid::fromArbitraryData(const QString& data)
{
    return fromArbitraryData(data.toUtf8());
}

Uuid Uuid::fromArbitraryData(std::string_view data)
{
    return fromArbitraryData(QByteArray::fromRawData(data.data(), (int) data.size()));
}

Uuid Uuid::createUuidFromPool(const QUuid &baseId, uint offset)
{
    static_assert(
        sizeof(uint) <= sizeof(decltype(QUuid::data1)),
        "Offset type must be not greater than storage field size.");
    QUuid result = baseId;
    result.data1 += offset;
    return Uuid(result);
}

Uuid Uuid::fromString(const std::string_view& value)
{
    Uuid result = Uuid::fromStringSafe(value);
    if (result.isNull()
        && value != "00000000-0000-0000-0000-000000000000"
        && value != "{00000000-0000-0000-0000-000000000000}")
    {
        return Uuid();
    }

    return result;
}

QString changedGuidByteOrder(const QString& guid)
{
    if (guid.length() != 36)
        return guid;

    QString result(guid);

    int replace[] = { 6, 7, 4, 5, 2, 3, 0, 1, 8, 11, 12, 9, 10, 13, 16, 17, 14, 15 };
    int max = sizeof(replace) / sizeof(replace[0]);
    for (int i = 0; i < max; i++)
        result[i] = guid[replace[i]];

    return result;
}

QDataStream& operator<<(QDataStream& s, const Uuid& id)
{
    return s << id.getQUuid();
}

QDebug operator<<(QDebug dbg, const Uuid& id)
{
    return dbg << id.getQUuid();
}

QDataStream& operator>>(QDataStream& s, Uuid& id)
{
    return s >> id.m_uuid;
}

} // namespace nx
