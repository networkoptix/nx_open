// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mac_address.h"

#include <algorithm>
#include <array>

#include <nx/utils/random.h>

namespace nx::utils {

constexpr std::array kAllowedDelimiters{'-', ':'};

MacAddress::MacAddress(const Data& bytes): m_data(bytes)
{
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
        static constexpr std::array kDelimiterIndices{2, 5, 8, 11, 14};

        // Check variant with delimiters. Only '-' or ':' are allowed.
        const auto delimiterIt = std::ranges::find_if(kAllowedDelimiters,
            [mac](char c)
            {
                return std::ranges::all_of(
                    kDelimiterIndices, [mac, c](int i) { return QLatin1Char(c) == mac[i]; });
            });

        if (delimiterIt == kAllowedDelimiters.end())
            return;

        // Check excessive delimiters.
        if (mac.count(QLatin1Char(*delimiterIt)) != kMacAddressLength - 1)
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
    std::ranges::copy_n(mac, kMacAddressLength, result.m_data.begin());
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
    return std::ranges::all_of(m_data, [](auto byte) { return byte == 0; });
}

const std::array<quint8, MacAddress::kMacAddressLength>& MacAddress::bytes() const
{
    return m_data;
}

QString MacAddress::toString() const
{
    return QString::fromStdString(toStdString());
}

std::string MacAddress::toStdString() const
{
    static constexpr std::string_view kHexDigits = "0123456789ABCDEF";

    std::string result(kMacAddressLength * 3 - 1, kAllowedDelimiters[0]);
    for (std::size_t i = 0; i < m_data.size(); ++i)
    {
        const auto byte = m_data[i];
        const auto resultIndex = i * 3;
        result[resultIndex] = kHexDigits[byte >> 4];
        result[resultIndex + 1] = kHexDigits[byte & 0x0F];
    }
    return result;
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
