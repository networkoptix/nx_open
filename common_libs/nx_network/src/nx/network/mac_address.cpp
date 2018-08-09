#include "mac_address.h"

#include <QtCore/QList>

namespace nx {
namespace network {

namespace {

static const QList<QChar> kAllowedDelimiters{'-', ':'};

} // namespace

QnMacAddress::QnMacAddress(std::initializer_list<quint8> bytes):
    QnMacAddress(QList<quint8>(bytes))
{
}

QnMacAddress::QnMacAddress(const QList<quint8>& bytes)
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

QnMacAddress::QnMacAddress(const QLatin1String& mac):
    QnMacAddress(QString(mac))
{
}

QnMacAddress::QnMacAddress(const QByteArray& mac):
    QnMacAddress(QString::fromLatin1(mac))
{
}

QnMacAddress::QnMacAddress(const QString& mac)
{
    if (mac.length() == kMacAddressLength * 2)
    {
        // Check variant without delimiters.
        decltype(m_data) data;
        for (int i = 0; i < kMacAddressLength; ++i)
        {
            auto segment = mac.midRef(i * 2, 2);
            bool canParse = false;
            const auto octet = segment.toInt(&canParse, 16);
            if (!canParse)
                return;

            data[i] = octet;
        }
        m_data = data;
    }
    else if (mac.length() == kMacAddressLength * 3 - 1)
    {
        // Check variant with delimiters. Only '-' or ':' are allowed.
        for (const QChar delimiter: kAllowedDelimiters)
        {
            if (mac.count(delimiter) == kMacAddressLength - 1)
            {
                decltype(m_data) data;
                for (int i = 0; i < kMacAddressLength; ++i)
                {
                    auto segment = mac.midRef(i * 3, 2);
                    bool canParse = false;
                    const auto byte = segment.toInt(&canParse, 16);
                    if (!canParse)
                        return;

                    data[i] = byte;
                }
                m_data = data;
                return;
            }
        }
    }
}

QnMacAddress QnMacAddress::fromRawData(const unsigned char* mac)
{
    QnMacAddress result;
    std::copy(mac, mac + kMacAddressLength, std::begin(result.m_data));
    return result;
}

bool QnMacAddress::isNull() const
{
    return std::all_of(m_data.cbegin(), m_data.cend(), [](auto byte) { return byte == 0; });
}

std::array<quint8, QnMacAddress::kMacAddressLength> QnMacAddress::bytes() const
{
    return m_data;
}

QString QnMacAddress::toString() const
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

bool QnMacAddress::operator==(const QnMacAddress& other) const
{
    return m_data == other.m_data;
}

bool QnMacAddress::operator!=(const QnMacAddress& other) const
{
    return m_data != other.m_data;
}

bool QnMacAddress::operator<(const QnMacAddress& other) const
{
    return m_data < other.m_data;
}

} // namespace network
} // namespace nx
