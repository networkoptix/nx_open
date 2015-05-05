#ifndef PCP_CLIENT_H
#define PCP_CLIENT_H

#include <QtNetwork/QNetworkInterface>

namespace pcp {

class Client
{
public:
    Client(const QHostAddress& address, int port);

    /** Maps external @var port to the internal */
    bool mapExternalPort();

private:
    QByteArray makeMapRequest();
    bool parseMapResponce(const QByteArray& buffer);

    QHostAddress m_address;
    int m_port;
    QHostAddress m_server;
    QByteArray m_nonce;
    quint32 m_lifeTime;

};

} // namespace pcp

#endif // PCP_CLIENT_H
