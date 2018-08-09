#pragma once

#include <array>

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QHash>

namespace nx {
namespace network {

class NX_NETWORK_API QnMacAddress
{
public:
    static constexpr int kMacAddressLength = 6;

    QnMacAddress() = default;
    explicit QnMacAddress(std::initializer_list<quint8> bytes);
    explicit QnMacAddress(const QList<quint8>& bytes);
    explicit QnMacAddress(const QString& mac);
    explicit QnMacAddress(const QLatin1String& mac);
    explicit QnMacAddress(const QByteArray& mac);

    static QnMacAddress fromRawData(const unsigned char* mac);

    bool isNull() const;

    std::array<quint8, kMacAddressLength> bytes() const;
    QString toString() const;

    bool operator==(const QnMacAddress& other) const;
    bool operator!=(const QnMacAddress& other) const;
    bool operator<(const QnMacAddress& other) const;

private:
    std::array<quint8, kMacAddressLength> m_data = {};
};

inline uint qHash(const QnMacAddress& value, uint seed = 0)
{
     return qHashRange(value.bytes().cbegin(), value.bytes().cend(), seed);
}

} // namespace network
} // namespace nx

Q_DECLARE_TYPEINFO(nx::network::QnMacAddress, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(nx::network::QnMacAddress)
