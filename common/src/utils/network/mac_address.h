#ifndef QN_MAC_ADDRESS_H
#define QN_MAC_ADDRESS_H

#include <QtCore/QString>
#include <QtCore/QMetaType>

class QnMacAddress
{
public:
    QnMacAddress();
    QnMacAddress(const unsigned char* mac);
    QnMacAddress(const QString& mac);
    ~QnMacAddress();

    bool isEmpty() const;

    QString toString() const;
    const unsigned char* toBytes() const;

    void setByte(int number, unsigned char val);
    unsigned char getByte(int number) const;

    QnMacAddress& operator = ( const QnMacAddress& other );
    bool operator == ( const QnMacAddress& other ) const;
    bool operator != ( const QnMacAddress& other ) const;
    bool operator < ( const QnMacAddress& other ) const;
    bool operator <= ( const QnMacAddress& other ) const;
    bool operator > ( const QnMacAddress& other ) const;
    bool operator >= ( const QnMacAddress& other ) const;

private:
    unsigned char m_data[6];


};

Q_DECLARE_METATYPE(QnMacAddress)
    
#endif //QN_MAC_ADDRESS_H