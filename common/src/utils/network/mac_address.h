#ifndef QN_MAC_ADDRESS_H
#define QN_MAC_ADDRESS_H

#include <boost/operators.hpp>

#include <QtCore/QString>
#include <QtCore/QMetaType>

class QnMacAddress: public boost::equality_comparable<QnMacAddress, boost::less_than_comparable<QnMacAddress> > {
public:
    QnMacAddress();
    QnMacAddress(const unsigned char *mac);
    QnMacAddress(const QString &mac);
    ~QnMacAddress();

    bool isNull() const;

    QString toString() const;
    const unsigned char *bytes() const;

    void setByte(int number, unsigned char val);
    unsigned char getByte(int number) const;

    QnMacAddress &operator= (const QnMacAddress &other);
    bool operator==(const QnMacAddress &other) const;
    bool operator<(const QnMacAddress &other) const;

	friend uint qHash(const QnMacAddress &value);

private:
    unsigned char m_data[6];
};

Q_DECLARE_TYPEINFO(QnMacAddress, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnMacAddress)
    
#endif //QN_MAC_ADDRESS_H