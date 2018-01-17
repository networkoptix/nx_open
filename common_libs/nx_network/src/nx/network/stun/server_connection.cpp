#include "server_connection.h"

#include "message_dispatcher.h"

namespace nx {
namespace network {
namespace stun {

ServerConnection::ServerConnection(
    nx::network::server::StreamConnectionHolder<ServerConnection>* socketServer,
    std::unique_ptr<AbstractStreamSocket> sock,
    const MessageDispatcher& dispatcher)
:
    base_type(socketServer, std::move(sock)),
    m_peerAddress(base_type::getForeignAddress()),
    m_dispatcher(dispatcher)
{
}

ServerConnection::~ServerConnection()
{
    // notify, that connection is being destruct
    // NOTE: this is needed only by weak_ptr holders to know, that this
    //       weak_ptr is not valid any more
    if( m_destructHandler )
        m_destructHandler();
}

void ServerConnection::sendMessage(
    nx::network::stun::Message message,
    std::function<void(SystemError::ErrorCode)> handler)
{
    base_type::sendMessage(
        std::move(message),
        std::move(handler));
}

nx::network::TransportProtocol ServerConnection::transportProtocol() const
{
    return nx::network::TransportProtocol::tcp;
}

SocketAddress ServerConnection::getSourceAddress() const
{
    return base_type::socket()->getForeignAddress();
}

void ServerConnection::addOnConnectionCloseHandler(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    registerCloseHandler(std::move(handler));
}

AbstractCommunicatingSocket* ServerConnection::socket()
{
    return base_type::socket().get();
}

void ServerConnection::close()
{
    auto socket = base_type::takeSocket();
    socket.reset();
}

void ServerConnection::setInactivityTimeout(
    boost::optional<std::chrono::milliseconds> value)
{
    base_type::setInactivityTimeout(value);
}

void ServerConnection::setDestructHandler(std::function< void() > handler)
{
    QnMutexLocker lk(&m_mutex);
    NX_ASSERT(!(handler && m_destructHandler), Q_FUNC_INFO,
        "Can not set new handler while previous is not removed");

    m_destructHandler = std::move(handler);
}

void ServerConnection::processMessage( Message message )
{
    switch( message.header.messageClass )
    {
        case MessageClass::request:
            switch( message.header.method )
            {
                case bindingMethod:
                    processBindingRequest( std::move( message ) );
                    break;

                default:
                    processCustomRequest( std::move( message ) );
                    break;
            }
            break;

        default:
            NX_ASSERT( false );  //not supported yet
    }

    // Message handler has closed connection.
    if (!socket())
        closeConnection(SystemError::noError);
}

void ServerConnection::processBindingRequest( Message message )
{
    Message response(stun::Header(
        MessageClass::successResponse,
        bindingMethod, std::move(message.header.transactionId)));

    response.newAttribute< stun::attrs::XorMappedAddress >(
        m_peerAddress.port, ntohl(m_peerAddress.address.ipV4()->s_addr));

    sendMessage(std::move(response), nullptr);
}

void ServerConnection::processCustomRequest( Message message )
{
    const auto messageHeader = message.header;

    if (m_dispatcher.dispatchRequest(shared_from_this(), std::move(message)))
        return;

    stun::Message response(stun::Header(
        stun::MessageClass::errorResponse,
        messageHeader.method,
        std::move(messageHeader.transactionId)));

    // TODO: verify with RFC
    response.newAttribute< stun::attrs::ErrorCode >(
        stun::error::notFound, "Method is not supported");

    sendMessage(std::move(response), nullptr);
}

} // namespace stun
} // namespace network
} // namespace nx
