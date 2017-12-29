#pragma once

#ifndef Q_MOC_RUN
#include <boost/operators.hpp>
#endif

#include <QtCore/QString>
#include <QtCore/QMetaType>

namespace nx {
namespace network {

class NX_NETWORK_API QnMacAddress:
    public boost::equality_comparable<QnMacAddress,
        boost::less_than_comparable<QnMacAddress>>
{
public:
    QnMacAddress();
    explicit QnMacAddress(const unsigned char* mac);
    explicit QnMacAddress(const QString& mac);
    explicit QnMacAddress(const QLatin1String& mac);
    explicit QnMacAddress(const QByteArray& mac);
    ~QnMacAddress() = default;

    bool isNull() const;

    QString toString() const;
    const unsigned char* bytes() const;

    void setByte(int number, unsigned char val);
    unsigned char getByte(int number) const;

    QnMacAddress &operator= (const QnMacAddress& other);
    bool operator==(const QnMacAddress& other) const;
    bool operator<(const QnMacAddress& other) const;

    friend uint NX_NETWORK_API qHash(const QnMacAddress& value);

private:
    void init(const QString &mac);

private:
    union
    {
        unsigned char bytes[6];
        struct
        {
            quint32 u32;
            quint16 u16;
        } uints;
    } m_data;
};

} // namespace network
} // namespace nx

Q_DECLARE_TYPEINFO(nx::network::QnMacAddress, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(nx::network::QnMacAddress)
