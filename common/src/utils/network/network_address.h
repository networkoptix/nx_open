#ifndef FULL_NETWORK_ADDRESS_H
#define FULL_NETWORK_ADDRESS_H

#include <QtNetwork/QHostAddress>

class QnNetworkAddress {
public:
    QnNetworkAddress();
    QnNetworkAddress(const QHostAddress &host, quint16 port);
    QnNetworkAddress(const QUrl &url);

    QHostAddress host() const;
    void setHost(const QHostAddress &host);

    quint16 port() const;
    void setPort(quint16 port);

    bool isValid() const;
    QUrl toUrl() const;

    bool operator ==(const QnNetworkAddress &other) const;

private:
    QHostAddress m_host;
    quint16 m_port;
};

uint qHash(const QnNetworkAddress &address, uint seed = 0);

Q_DECLARE_METATYPE(QnNetworkAddress)

#endif // FULL_NETWORK_ADDRESS_H
