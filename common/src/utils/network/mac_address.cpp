#include "mac_address.h"
#include "nettools.h"




QnMacAddress::QnMacAddress()
{
    memset(m_data, 0, 6);
}

QnMacAddress::QnMacAddress(const unsigned char* mac)
{
    memcpy(m_data, mac, 6);
}

QnMacAddress::QnMacAddress(const QString& mac)
{
    if (mac.contains(QLatin1Char('-')))
        MACsToByte(mac, m_data);
    else
        MACsToByte2(mac, m_data);

}

QnMacAddress::~QnMacAddress()
{
    
}

bool QnMacAddress::isEmpty() const
{
    for(int i = 0; i < 6; ++i)
    {
        if (m_data[i]!=0)
            return false;
    }

    return true;
}

QString QnMacAddress::toString() const
{
    return MACToString(m_data);
}

const unsigned char* QnMacAddress::toBytes() const
{
    return m_data;
}

void QnMacAddress::setByte(int number, unsigned char val)
{
    m_data[number] = val;
}

unsigned char QnMacAddress::getByte(int number) const
{
    return m_data[number];
}

QnMacAddress& QnMacAddress::operator = ( const QnMacAddress& other )
{
    memcpy(m_data, other.m_data, 6);
    return *this;
}

bool QnMacAddress::operator == ( const QnMacAddress& other ) const
{
    return (memcmp(m_data, other.m_data, 6) == 0);
}

bool QnMacAddress::operator != ( const QnMacAddress& other ) const
{
    return (memcmp(m_data, other.m_data, 6) != 0);
}

bool QnMacAddress::operator < ( const QnMacAddress& other ) const
{
    return toString() < other.toString();
}

bool QnMacAddress::operator <= ( const QnMacAddress& other ) const
{
    return toString() <= other.toString();
}

bool QnMacAddress::operator > ( const QnMacAddress& other ) const
{
    return toString() > other.toString();
}

bool QnMacAddress::operator >= ( const QnMacAddress& other ) const
{
    return toString() >= other.toString();
}

