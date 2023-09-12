// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uuid.h"

#include <type_traits>

#include <QtCore/QCryptographicHash>

#include <nx/utils/log/assert.h>

QnUuid::QnUuid()
{
}

QnUuid::QnUuid(const char* text)
{
    const auto& qByteArrayText =
        text ? QByteArray::fromRawData(text, static_cast<int>(strlen(text))) : QByteArray();
    NX_ASSERT(isUuidString(qByteArrayText), text);
    m_uuid = QUuid(qByteArrayText);
}

QnUuid::QnUuid(const QString& text): m_uuid(text)
{
    NX_ASSERT(isUuidString(text), text);
}

QnUuid::QnUuid(const QByteArray& text): m_uuid(text)
{
    NX_ASSERT(isUuidString(text), text);
}

QnUuid::QnUuid(const std::string& text):
    m_uuid(QByteArray::fromStdString(text))
{
    NX_ASSERT(isUuidString(text), text);
}

QnUuid::QnUuid(const QUuid& uuid): m_uuid(uuid)
{
}

bool QnUuid::isUuidString(const QByteArray& data)
{
    return data.isEmpty()
        || data.size() == 36 //< xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        || data.size() == 38; //< {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
}

bool QnUuid::isUuidString(const QString& data)
{
    return data.isEmpty()
        || data.size() == 36 //< xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        || data.size() == 38; //< {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
}

bool QnUuid::isUuidString(const std::string& data)
{
    return data.empty()
        || data.size() == 36 //< xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        || data.size() == 38; //< {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
}

const QUuid& QnUuid::getQUuid() const
{
    return m_uuid;
}

bool QnUuid::isNull() const
{
    return m_uuid.isNull();
}

const QByteArray QnUuid::toByteArray() const
{
    return m_uuid.toByteArray();
}

const QByteArray QnUuid::toRfc4122() const
{
    return m_uuid.toRfc4122();
}

const QString QnUuid::toString() const
{
    return m_uuid.toString();
}

QString QnUuid::toSimpleString() const
{
    return m_uuid.toString(QUuid::WithoutBraces);
}

QByteArray QnUuid::toSimpleByteArray() const
{
    return m_uuid.toByteArray(QUuid::WithoutBraces);
}

std::string QnUuid::toStdString() const
{
    const auto& byteArray = toByteArray();
    return std::string(byteArray.constData(), byteArray.size());
}

std::string QnUuid::toSimpleStdString() const
{
    return toSimpleByteArray().toStdString();
}

QUuid QnUuid::toQUuid() const
{
    return m_uuid;
}

bool QnUuid::operator<(const QnUuid& other) const
{
    return m_uuid < other.m_uuid;
}

bool QnUuid::operator>(const QnUuid& other) const
{
    return m_uuid > other.m_uuid;
}

QnUuid QnUuid::fromRfc4122(const QByteArray& bytes)
{
    QnUuid _uuid;
    _uuid.m_uuid = QUuid::fromRfc4122(bytes);
    return _uuid;
}

QnUuid QnUuid::fromHardwareId(const QString& hwid)
{
    if (hwid.length() != 34)
        return QnUuid();

    return QnUuid(QString::fromLatin1("%1-%2-%3-%4-%5")
        .arg(hwid.mid(2, 8)).arg(hwid.mid(10, 4)).arg(hwid.mid(14, 4))
        .arg(hwid.mid(18, 4)).arg(hwid.mid(22, 12)));
}

QnUuid QnUuid::createUuid()
{
    QnUuid _uuid;
    _uuid.m_uuid = QUuid::createUuid();
    return _uuid;
}

QnUuid QnUuid::fromStringSafe(const QString& uuid)
{
    return QnUuid(QUuid(uuid));
}

QnUuid QnUuid::fromStringSafe(const QByteArray& uuid)
{
    return QnUuid(QUuid(uuid));
}

QnUuid QnUuid::fromStringSafe(const char* uuid)
{
    return QnUuid(QUuid(uuid));
}

QnUuid QnUuid::fromStringSafe(const std::string_view& uuid)
{
    return fromStringSafe(QByteArray::fromRawData(uuid.data(), (int) uuid.size()));
}

QnUuid QnUuid::fromArbitraryData(const QByteArray& data)
{
    QCryptographicHash md5Hash(QCryptographicHash::Md5);
    md5Hash.addData(data);
    QByteArray bytes = md5Hash.result();
    return fromRfc4122(bytes);
}

QnUuid QnUuid::fromArbitraryData(const QString& data)
{
    return fromArbitraryData(data.toUtf8());
}

QnUuid QnUuid::fromArbitraryData(std::string_view data)
{
    return fromArbitraryData(QByteArray::fromRawData(data.data(), (int) data.size()));
}

QnUuid QnUuid::createUuidFromPool(const QUuid &baseId, uint offset)
{
    static_assert(
        sizeof(uint) <= sizeof(decltype(QUuid::data1)),
        "Offset type must be not greater than storage field size.");
    QUuid result = baseId;
    result.data1 += offset;
    return QnUuid(result);
}

QnUuid QnUuid::fromString(const std::string_view& value)
{
    QnUuid result = QnUuid::fromStringSafe(value);
    if (result.isNull()
        && value != "00000000-0000-0000-0000-000000000000"
        && value != "{00000000-0000-0000-0000-000000000000}")
    {
        return QnUuid();
    }

    return result;
}

namespace nx::utils {

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

} // namespace nx::utils

size_t qHash(const QnUuid& uuid, size_t seed) noexcept
{
    return qHash(uuid.getQUuid()) ^ seed;
}

QDataStream& operator<<(QDataStream& s, const QnUuid& id)
{
    return s << id.getQUuid();
}

QDebug operator<<(QDebug dbg, const QnUuid& id)
{
    return dbg << id.getQUuid();
}

QDataStream& operator>>(QDataStream& s, QnUuid& id)
{
    return s >> id.m_uuid;
}
