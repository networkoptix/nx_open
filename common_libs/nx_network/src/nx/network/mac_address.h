#pragma once

#include <array>

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QHash>

namespace nx {
namespace network {

class NX_NETWORK_API MacAddress
{
public:
    static constexpr int kMacAddressLength = 6;
    using Data = std::array<quint8, kMacAddressLength>;

    MacAddress() = default;
    explicit MacAddress(const Data& bytes);
    explicit MacAddress(const QString& mac);
    explicit MacAddress(const QLatin1String& mac);
    explicit MacAddress(const QByteArray& mac);

    static MacAddress fromRawData(const unsigned char* mac);

    bool isNull() const;

    Data bytes() const;
    QString toString() const;

    bool operator==(const MacAddress& other) const;
    bool operator!=(const MacAddress& other) const;
    bool operator<(const MacAddress& other) const;

private:
    Data m_data = {};
};

inline uint qHash(const MacAddress& value, uint seed = 0)
{
     return qHashRange(value.bytes().cbegin(), value.bytes().cend(), seed);
}

} // namespace network
} // namespace nx

Q_DECLARE_TYPEINFO(nx::network::MacAddress, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(nx::network::MacAddress)
