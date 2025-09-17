// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "uuid.h"

#include <regex>
#include <type_traits>

#include <QtCore/QCryptographicHash>

#include <nx/reflect/json/deserializer.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/log/assert.h>

namespace {

constexpr std::string_view kNullUuid = "00000000-0000-0000-0000-000000000000";
constexpr std::string_view kNullUuidWithBraces = "{00000000-0000-0000-0000-000000000000}";

bool isValidNullUuidString(const std::string_view& data)
{
    return data == kNullUuid || data == kNullUuidWithBraces;
}

// Qt 6.8.0 broke or changed QUuid sorting order. This function implements the old behavior.
Qt::strong_ordering compare(const QUuid& left, const QUuid& right)
{
    if (const auto c = Qt::compareThreeWay(left.variant(), right.variant()); !is_eq(c))
        return c;
    if (const auto c = Qt::compareThreeWay(left.data1, right.data1); !is_eq(c))
        return c;
    if (const auto c = Qt::compareThreeWay(left.data2, right.data2); !is_eq(c))
        return c;
    if (const auto c = Qt::compareThreeWay(left.data3, right.data3); !is_eq(c))
        return c;

    int c = std::memcmp(left.data4, right.data4, sizeof(left.data4));
    return Qt::compareThreeWay(c, 0);
}

} // namespace

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

bool Uuid::isUuidString(const std::string_view& data)
{
    return data.empty()
        || data.size() == 36 //< xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        || data.size() == 38; //< {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
}

inline bool validateUuid(const std::string& s) {
    static const std::regex uuid_regex(
        "^\\{?[0-9a-f]{8}-[0-9a-f]{4}-[0-5][0-9a-f]{3}-[089ab][0-9a-f]{3}-[0-9a-f]{12}\\}?$",
        std::regex_constants::icase
    );
    return std::regex_match(s, uuid_regex);
}

bool Uuid::isValidUuidString(const QByteArray& data)
{
    return validateUuid(std::string(data));
}

bool Uuid::isValidUuidString(const QString& data)
{
    return validateUuid(data.toStdString());
}

bool Uuid::isValidUuidString(const std::string& data)
{
    return validateUuid(data);
}

bool Uuid::isValidUuidString(const std::string_view& data)
{
    return validateUuid(std::string(data));
}

const QUuid& Uuid::getQUuid() const
{
    return m_uuid;
}

bool Uuid::isNull() const
{
    return m_uuid.isNull();
}

const QByteArray Uuid::toByteArray(QUuid::StringFormat format) const
{
    return m_uuid.toByteArray(format);
}

const QByteArray Uuid::toRfc4122() const
{
    return m_uuid.toRfc4122();
}

const QString Uuid::toString(QUuid::StringFormat format) const
{
    return m_uuid.toString(format);
}

QString Uuid::toSimpleString() const
{
    return m_uuid.toString(QUuid::WithoutBraces);
}

QByteArray Uuid::toSimpleByteArray() const
{
    return m_uuid.toByteArray(QUuid::WithoutBraces);
}

std::string Uuid::toStdString(QUuid::StringFormat format) const
{
    const auto& byteArray = toByteArray(format);
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

std::optional<std::chrono::milliseconds> Uuid::timeSinceEpoch() const
{
    if (m_uuid.version() != QUuid::UnixEpoch)
        return {};
    uint64_t msNumber = (uint64_t(m_uuid.data1) << 16) | m_uuid.data2;
    return std::chrono::milliseconds(msNumber);
}

std::strong_ordering Uuid::operator<=>(const Uuid& other) const
{
    const Qt::strong_ordering ord = compare(m_uuid, other.m_uuid);
    #if defined(ANDROID) || defined(__ANDROID__)
        // Android does not support conversions between Qt and std orderings.
        std::strong_ordering result = std::strong_ordering::equivalent;
        if (ord == Qt::strong_ordering::less)
            result = std::strong_ordering::less;
        else if (ord == Qt::strong_ordering::equivalent)
            result = std::strong_ordering::equivalent;
        else if (ord == Qt::strong_ordering::equal)
            result = std::strong_ordering::equal;
        else if (ord == Qt::strong_ordering::greater)
            result = std::strong_ordering::greater;
        else
            NX_ERROR(this, "Impossible Qt::strong_ordering value");
    #else
        std::strong_ordering result = static_cast<std::strong_ordering>(ord);
    #endif
    return result;
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

Uuid Uuid::createUuidV7(std::chrono::milliseconds timeSinceEpoch)
{
    Uuid result;
    result.m_uuid = QUuid::createUuidV7();
    uint64_t msNumber = timeSinceEpoch.count();
    // Do the same is in https://code.qt.io/cgit/qt/qtbase.git/tree/src/corelib/plugin/quuid_p.h
    // Counter part of the time is from qt system_clock nanosec part is left as is.
    result.m_uuid.data1 = (uint) (msNumber >> 16);
    result.m_uuid.data2 = (ushort) msNumber;
    return result;
}

Uuid Uuid::fromString(const std::string_view& value)
{
    return Uuid::fromStringSafe(value);
}

Uuid Uuid::fromStringWithCheck(const std::string_view& str, bool* ok)
{
    Uuid result = Uuid::fromStringSafe(str);
    if (ok)
        *ok = str.empty() || !result.isNull() || isValidNullUuidString(str);

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

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context, Uuid* data)
{
    if (!context.value.IsString())
    {
        return {false,
            "String value is expected",
            nx::reflect::json_detail::getStringRepresentation(context.value)};
    }

    if (!Uuid::isUuidString(
            std::string_view{context.value.GetString(), (size_t) context.value.GetStringLength()}))
    {
        return {
            false, "Not a UUID", nx::reflect::json_detail::getStringRepresentation(context.value)};
    }

    *data = Uuid::fromStringSafe(
        std::string_view{context.value.GetString(), context.value.GetStringLength()});
    return {};
}

} // namespace nx
