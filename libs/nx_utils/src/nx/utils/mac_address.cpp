// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_address.h"

#include <QtCore/QList>

#include <nx/utils/random.h>

namespace nx::utils {

namespace {

static const QList<QChar> kAllowedDelimiters{'-', ':'};

} // namespace

MacAddress::MacAddress(const Data& bytes)
{
    int index = 0;
    for (auto byte: bytes)
    {
        m_data[index] = byte;
        ++index;
        if (index == kMacAddressLength)
            break;
    }
}

MacAddress::MacAddress(const QLatin1String& mac):
    MacAddress(QString(mac))
{
}

MacAddress::MacAddress(const QByteArray& mac):
    MacAddress(QString::fromLatin1(mac))
{
}

MacAddress::MacAddress(const QStringView& mac)
{
    // Check string format.
    const bool hasDelimiters = mac.length() == kMacAddressLength * 3 - 1;
    if (hasDelimiters)
    {
        QChar delimiter;

        static const std::list<int> kDelimiterIndices{2,5,8,11,14};

        // Check variant with delimiters. Only '-' or ':' are allowed.
        for (const QChar& c: kAllowedDelimiters)
        {
            if (std::all_of(kDelimiterIndices.cbegin(), kDelimiterIndices.cend(),
                [mac, c](int index) { return mac[index] == c; }))
            {
                delimiter = c;
                break;
            }
        }

        if (delimiter.isNull())
            return;

        // Check excessive delimiters.
        if (mac.count(delimiter) != kMacAddressLength - 1)
            return;
    }
    else if (mac.length() != kMacAddressLength * 2)
    {
        // Invalid length of string without delimiters.
        return;
    }

    const int segmentOffset = hasDelimiters ? 3 : 2;
    Data data;
    for (int i = 0; i < kMacAddressLength; ++i)
    {
        auto segment = mac.mid(i * segmentOffset, 2);

        // Segments like '+4' can be parsed, but this is definitely non-standard mac address.
        if (segment[0] == '+')
            return;

        bool canParse = false;
        const auto octet = segment.toInt(&canParse, 16);
        if (octet < 0 || !canParse)
            return;

        data[i] = octet;
    }
    m_data = data;
}

MacAddress::MacAddress(const std::string_view& mac):
    MacAddress(QString::fromUtf8(mac.data(), (int) mac.size()))
{
    // TODO: #akolesnikov Move implementation to this constructor since it will be more efficient.
}

MacAddress MacAddress::fromRawData(const unsigned char* mac)
{
    MacAddress result;
    std::copy(mac, mac + kMacAddressLength, std::begin(result.m_data));
    return result;
}

MacAddress MacAddress::random()
{
    MacAddress result;
    for (auto& byte: result.m_data)
        byte = (uint8_t) nx::utils::random::number<int>(0, std::numeric_limits<uint8_t>::max());
    return result;
}

bool MacAddress::isNull() const
{
    return std::all_of(m_data.cbegin(), m_data.cend(), [](auto byte) { return byte == 0; });
}

const std::array<quint8, MacAddress::kMacAddressLength>& MacAddress::bytes() const
{
    return m_data;
}

QString MacAddress::toString() const
{
    QStringList bytes;
    std::transform(
        m_data.cbegin(),
        m_data.cend(),
        std::back_inserter(bytes),
        [](auto byte)
        {
            return QString("%1").arg((uint)byte, 2, 16, QChar('0')).toUpper();
        });
    return bytes.join(kAllowedDelimiters[0]);
}

bool MacAddress::operator==(const MacAddress& other) const
{
    return m_data == other.m_data;
}

bool MacAddress::operator!=(const MacAddress& other) const
{
    return m_data != other.m_data;
}

bool MacAddress::operator<(const MacAddress& other) const
{
    return m_data < other.m_data;
}

} // namespace nx::utils
