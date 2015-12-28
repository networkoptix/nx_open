/**********************************************************
* Dec 25, 2015
* akolesnikov
***********************************************************/

#include "connection.h"


namespace nx {
namespace hpm {

#if 0

TcpServerConnection::~TcpServerConnection()
{
}

void TcpServerConnection::sendMessage(nx::stun::Message message)
{
    auto strongRef = m_connection.lock();
    assert(strongRef);
    strongRef->sendMessage(std::move(message));
}

nx::network::TransportProtocol TcpServerConnection::transportProtocol() const
{
    return nx::network::TransportProtocol::tcp;
}

SocketAddress TcpServerConnection::getSourceAddress() const
{
    auto strongRef = m_connection.lock();
    assert(strongRef);
    return strongRef->socket()->getForeignAddress();
}

void TcpServerConnection::registerCloseHandler(std::function<void()> handler)
{
    m_connectionTerminatedHandler = std::move(handler);
}

std::shared_ptr<TcpServerConnection> TcpServerConnection::create(
    std::shared_ptr< nx::stun::ServerConnection > connection)
{
    auto instance = std::shared_ptr<TcpServerConnection>(new TcpServerConnection());
    instance->setConnection(std::move(connection));
    return instance;
}

TcpServerConnection::TcpServerConnection()
{
}

void TcpServerConnection::setConnection(
    std::shared_ptr< nx::stun::ServerConnection > connection)
{
    connection->registerCloseHandler(
        std::bind(&TcpServerConnection::connectionIsAboutToClose, this, shared_from_this()));
    m_connection = std::move(connection);
}

void TcpServerConnection::connectionIsAboutToClose(
    std::shared_ptr<TcpServerConnection> strongThisRef)
{
    if (m_connectionTerminatedHandler)
    {
        auto localHandler = std::move(m_connectionTerminatedHandler);
        localHandler();
    }
}

#endif

}   //hpm
}   //nx
