#include "client.h"

#include <QtCore/QMutex>
#include <QtCore/QDateTime>

#include <queue>

#include <utils/network/socket_factory.h>

#include "messaging.h"

static const int AFORT_COUNT = 3;
static const quint32 LIFETIME = 1 * 60 * 60; // 1 hour

namespace pcp {

Client::Client(const QHostAddress& address, int port)
    : m_address(address), m_port(m_port)
    , m_server()
    , m_lifeTime(0)
{
}

bool Client::mapExternalPort()
{
    if (QDateTime::currentDateTime().toTime_t() < m_lifeTime)
        return true; // already forwarded

    QScopedPointer<AbstractDatagramSocket> socket(SocketFactory::createDatagramSocket());
    m_nonce = makeRandomNonce();

    SocketAddress serverPort(HostAddress(m_server.toString()), SERVER_PORT);
    QByteArray request = makeMapRequest();
    if (request.size() != socket->sendTo(request.data(), request.size(), serverPort))
        return false;

    SocketAddress clientPort(HostAddress(m_address.toString()), SERVER_PORT);
    for (int aforts = AFORT_COUNT; aforts; --aforts)
    {
        QByteArray response;
        socket->recvFrom(response.data(), response.size(), &clientPort);
        if (parseMapResponce(response))
            return true;
    }

    return false;
}

QByteArray Client::makeMapRequest()
{
    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);

    RequestHeader header;
    header.version = VERSION;
    header.opcode = Opcode::MAP;
    header.lifeTime = LIFETIME;
    header.clientIp = m_address.toIPv4Address();
    stream << header;

    MapMessage message;
    message.nonce = m_nonce;
    message.protocol = 0; // any
    message.externalPort = m_port;
    message.externalIp = 0; // any
    message.externalPort = m_port;
    stream << message;

    return request;
}

bool Client::parseMapResponce(const QByteArray& buffer)
{
    if (buffer.size() <= 4)
        return false;

    QDataStream stream(buffer);

    ResponseHeadeer header;
    stream >> header;

    if (header.version != VERSION) return false;
    if (header.opcode != Opcode::MAP) return false;
    if (header.resultCode != ResultCode::SUCCESS) return false;

    MapMessage message;
    stream >> message;

    if (message.internalPort != m_port) return false;
    if (message.externalPort != m_port) return false;

    m_lifeTime = QDateTime::currentDateTime().toTime_t() + header.lifeTime;
    return true;
}

} // namespace pcp
