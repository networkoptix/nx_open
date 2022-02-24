// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "router_pcp.h"

#include "messaging.h"

#include <QtCore/QElapsedTimer>
#include <QDateTime>

static const quint32 LIFETIME = 1 * 60 * 60;
static const int RESPONSE_WAIT = 1000;
static const int RESPONSE_BUFFER = 1024;

namespace nx {
namespace network {
namespace pcp {

Router::Router(const HostAddress& address)
    : m_serverAddress(address, SERVER_PORT)
    , m_clientSocket(SocketFactory::createDatagramSocket())
    , m_serverSocket(SocketFactory::createDatagramSocket())
{
    m_clientSocket->bind(SocketAddress(HostAddress(), CLIENT_PORT));
    m_serverSocket->setDestAddr(m_serverAddress);
}

void Router::mapPort(Mapping& mapping)
{
    QMutexLocker lock(&m_mutex);

    auto request = makeMapRequest(mapping);
    if (!m_serverSocket->send(nx::Buffer(std::move(request))))
        return;

    QElapsedTimer timer;
    while (timer.elapsed() < RESPONSE_WAIT)
    {
        QByteArray response(RESPONSE_BUFFER, Qt::Uninitialized);
        SocketAddress server;
        int recv = m_clientSocket->recvFrom(response.data(), response.size(), &server);
        if (server != m_serverAddress) continue;
        if (recv < 4) continue;

        response.reserve(recv);
        if (parseMapResponse(response, mapping))
            return;
    }
}

QByteArray Router::makeMapRequest(Mapping& mapping)
{
    if (!mapping.nonce.size())
        mapping.nonce = makeRandomNonce();
    if (!mapping.external.port)
        mapping.external.port = mapping.internal.port;

    QByteArray request;
    QDataStream stream(&request, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    RequestHeader header;
    header.version = VERSION;
    header.opcode = Opcode::MAP;
    header.lifeTime = LIFETIME;

    const auto ipV6Addr = *mapping.internal.address.ipV6().first;
    header.clientIp = QByteArray(
        reinterpret_cast<const char*>(&ipV6Addr),
        sizeof(ipV6Addr));

    MapMessage message;
    message.nonce = mapping.nonce;
    message.protocol = 0; // all
    message.internalPort = mapping.internal.port;
    message.externalPort = mapping.external.port;
    message.externalIp = QByteArray(16, 0); // unknown

    stream << header << message;
    return request;
}

bool Router::parseMapResponse(const QByteArray& response, Mapping& mapping)
{
    QDataStream stream(response);
    stream.setByteOrder(QDataStream::LittleEndian);

    ResponseHeadeer header;
    stream >> header;
    if (header.opcode != Opcode::MAP) return false;
    if (header.resultCode != ResultCode::SUCCESS)
    {
        qDebug() << "Router::parseMapResponse: resultCode ="
                 << static_cast<int>(header.resultCode);
        return true;
    }

    MapMessage message;
    stream >> message;
    if (message.nonce != mapping.nonce) return false;
    if (message.internalPort != mapping.internal.port) return false;

    in6_addr addr;
    NX_CRITICAL(message.externalIp.size() == sizeof(addr));
    std::memcpy(&addr, message.externalIp.data(), sizeof(addr));

    mapping.lifeTime = QDateTime::currentDateTime().toSecsSinceEpoch() + header.lifeTime;
    mapping.external = SocketAddress(HostAddress(addr), message.externalPort);
    return true;
}

} // namespace pcp
} // namespace network
} // namespace nx
