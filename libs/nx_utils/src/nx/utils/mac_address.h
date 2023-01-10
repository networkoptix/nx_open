// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <string_view>

#include <QtCore/QString>
#include <QtCore/QHash>

namespace nx::utils {

class NX_UTILS_API MacAddress
{
public:
    static constexpr int kMacAddressLength = 6;
    using Data = std::array<quint8, kMacAddressLength>;

    MacAddress() = default;
    explicit MacAddress(const Data& bytes);
    explicit MacAddress(const QStringView& mac);
    explicit MacAddress(const QLatin1String& mac);
    explicit MacAddress(const QByteArray& mac);
    MacAddress(const std::string_view& mac);

    static MacAddress fromRawData(const unsigned char* mac);
    static MacAddress random();

    bool isNull() const;

    const Data& bytes() const;
    QString toString() const;

    bool operator==(const MacAddress& other) const;
    bool operator!=(const MacAddress& other) const;
    bool operator<(const MacAddress& other) const;

private:
    Data m_data = {};
};

inline size_t qHash(const MacAddress& value, size_t seed = 0)
{
    const auto data = value.bytes();
    return qHashRange(data.cbegin(), data.cend(), seed);
}

} // namespace nx::utils
