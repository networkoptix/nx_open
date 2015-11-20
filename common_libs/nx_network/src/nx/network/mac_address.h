#ifndef QN_MAC_ADDRESS_H
#define QN_MAC_ADDRESS_H

#include <boost/operators.hpp>

#include <QtCore/QString>
#include <QtCore/QMetaType>

class QnMacAddress: public boost::equality_comparable<QnMacAddress, boost::less_than_comparable<QnMacAddress> > {
public:
    QnMacAddress();
    explicit QnMacAddress(const unsigned char *mac);
    explicit QnMacAddress(const QString &mac);
    explicit QnMacAddress(const QLatin1String &mac);
    explicit QnMacAddress(const QByteArray &mac);
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
    void init(const QString &mac);

private:
    union
    {
        unsigned char bytes[6];
        struct {
            quint32 u32;
            quint16 u16;
        } uints;
    } m_data;
};

Q_DECLARE_TYPEINFO(QnMacAddress, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnMacAddress)
    
#endif //QN_MAC_ADDRESS_H