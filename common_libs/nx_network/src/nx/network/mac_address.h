#pragma once

#include <array>

#include <QtCore/QString>
#include <QtCore/QMetaType>

namespace nx {
namespace network {

class NX_NETWORK_API QnMacAddress
{
public:
    static constexpr int kMacAddressLength = 6;

    QnMacAddress() = default;
    explicit QnMacAddress(std::initializer_list<quint8> bytes);
    explicit QnMacAddress(const QList<quint8>& bytes);
    explicit QnMacAddress(const unsigned char* mac);
    explicit QnMacAddress(const QString& mac);
    explicit QnMacAddress(const QLatin1String& mac);
    explicit QnMacAddress(const QByteArray& mac);

    bool isNull() const;

    std::array<quint8, kMacAddressLength> bytes() const;
    QString toString() const;

    bool operator==(const QnMacAddress& other) const;
    bool operator!=(const QnMacAddress& other) const;
    bool operator<(const QnMacAddress& other) const;

    friend uint NX_NETWORK_API qHash(const QnMacAddress& value, uint seed = 0);

private:
    std::array<quint8, kMacAddressLength> m_data = {};
};

} // namespace network
} // namespace nx

Q_DECLARE_TYPEINFO(nx::network::QnMacAddress, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(nx::network::QnMacAddress)
