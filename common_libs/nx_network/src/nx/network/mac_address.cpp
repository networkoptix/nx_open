#include "mac_address.h"

#include "nettools.h"

QnMacAddress::QnMacAddress()
{
    memset(m_data.bytes, 0, 6);
}

QnMacAddress::QnMacAddress(const unsigned char* mac)
{
    memcpy(m_data.bytes, mac, 6);
}

QnMacAddress::QnMacAddress(const QLatin1String& mac)
{
    init(mac);
}

QnMacAddress::QnMacAddress(const QByteArray& mac)
{
    init(QLatin1String(mac));
}

QnMacAddress::QnMacAddress(const QString& mac)
{
    init(mac);
}

void QnMacAddress::init(const QString& mac)
{
    if (mac.contains(QLatin1Char('-')))
        MACsToByte(mac, m_data.bytes, '-');
    else if (mac.contains(QLatin1Char(':')))
        MACsToByte(mac, m_data.bytes, ':');
    else
        MACsToByte2(mac, m_data.bytes);
}

bool QnMacAddress::isNull() const
{
    for (int i = 0; i < 6; ++i)
        if (m_data.bytes[i] != 0)
            return false;

    return true;
}

QString QnMacAddress::toString() const
{
    return MACToString(m_data.bytes);
}

const unsigned char* QnMacAddress::bytes() const
{
    return m_data.bytes;
}

void QnMacAddress::setByte(int number, unsigned char val)
{
    m_data.bytes[number] = val;
}

unsigned char QnMacAddress::getByte(int number) const
{
    return m_data.bytes[number];
}

QnMacAddress& QnMacAddress::operator=(const QnMacAddress& other)
{
    memcpy(m_data.bytes, other.m_data.bytes, 6);
    return *this;
}

bool QnMacAddress::operator==(const QnMacAddress& other) const
{
    return memcmp(m_data.bytes, other.m_data.bytes, 6) == 0;
}

bool QnMacAddress::operator<(const QnMacAddress& other) const
{
    return memcmp(m_data.bytes, other.m_data.bytes, 6) < 0;
}

uint qHash(const QnMacAddress& value)
{
    static_assert(
        sizeof(value.m_data.bytes) == sizeof(quint32) + sizeof(quint16),
        "Invalid MAC address data size.");

    return value.m_data.uints.u32 * 863 + value.m_data.uints.u16;
}
