#ifndef PCP_CLIENT_H
#define PCP_CLIENT_H

#include <QtNetwork/QNetworkInterface>

#include <utils/network/socket_factory.h>

namespace pcp {

/** PCP Client to work with single network interface */
class InterfaceClient
{
public:
    /** @param address network interface ip */
    InterfaceClient(const QHostAddress& address);

    /** Mapping callback, informs external address and port, NULL otherwise */
    typedef std::function<void(const QHostAddress&, quint16,
                               const QHostAddress&, quint16)> MapCallback;

    /** Maps internal @param port to the same port on NAT async */
    void mapPort(quint16 port, const MapCallback& callback);

protected:
    struct ExternalMapping
    {
         QByteArray nonce;
         QHostAddress address;
         quint16 port;
         quint32 lifeTime;
    };

    QByteArray makeMapRequest(int port, const QByteArray& nonce);
    ExternalMapping parseMapResponce(const QByteArray& buffer);

private:
    QMutex s_mutex;
    QHostAddress m_interface;
    QHostAddress m_server;
    QScopedPointer<AbstractDatagramSocket> m_sender;
    QScopedPointer<AbstractDatagramSocket> m_reciever;
    std::map<quint32, ExternalMapping> m_mappings;
};

/** PCP Client to work with multiplie network interfaces */
class MultiClient
{
public:
    static MultiClient& instance();

    // TODO boost::signals
    Guard subscribe(const InterfaceClient::MapCallback& callback);

    /** Mapps @param port on @param address to the same port on NAT async */
    Guard mapPort(const QHostAddress& address, quint16 port);

private:
    MultiClient();

    QMutex s_mutex;
    std::map<QHostAddress, InterfaceClient> m_client;
};




// ---

class Client
{
public:
    Client(const QHostAddress& address, int port);

    /** Maps external @var port to the internal */
    void mapExternalPort();

signals:
    void portMapped(const QHostAddress& address);

protected:
    QByteArray makeMapRequest(int port);
    bool parseMapResponce(const QByteArray& buffer);

private:
    QHostAddress m_address;
    int m_port;
    QHostAddress m_server;
    QByteArray m_nonce;
    quint32 m_lifeTime;

};

} // namespace pcp

#endif // PCP_CLIENT_H
