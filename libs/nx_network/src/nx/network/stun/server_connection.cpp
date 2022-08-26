// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_connection.h"

#include <nx/network/socket_global.h>

#include "abstract_message_handler.h"

namespace nx::network::stun {

ServerConnection::ServerConnection(
    std::unique_ptr<AbstractStreamSocket> sock,
    AbstractMessageHandler* messageHandler,
    nx::utils::stree::StringAttrDict attrs)
    :
    base_type(std::move(sock)),
    m_peerAddress(base_type::getForeignAddress()),
    m_messageHandler(messageHandler),
    m_attrs(std::move(attrs))
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().stunServerConnectionCount;
}

ServerConnection::~ServerConnection()
{
    // Notifying that connection is being destroyed
    // NOTE: This is needed only by weak_ptr holders to know, that this
    // weak_ptr is not valid any more.
    if (m_destructHandler)
        m_destructHandler();

    --SocketGlobals::instance().debugCounters().stunServerConnectionCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

void ServerConnection::sendMessage(
    nx::network::stun::Message message,
    std::function<void(SystemError::ErrorCode)> handler)
{
    base_type::sendMessage(std::move(message), std::move(handler));
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
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    registerCloseHandler(
        [handler = std::move(handler)](auto reason, auto connectionDestroyed)
        {
            if (connectionDestroyed)
            {
                NX_DEBUG(typeid(ServerConnection),
                    "invoking close handler on destroyed connection");
            }
            handler(reason);
        });
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
    std::optional<std::chrono::milliseconds> value)
{
    base_type::setInactivityTimeout(value);
}

void ServerConnection::setDestructHandler(std::function<void()> handler)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    NX_ASSERT(!(handler && m_destructHandler),
        "Can not set new handler while previous is not removed");

    m_destructHandler = std::move(handler);
}

void ServerConnection::processMessage(Message message)
{
    switch (message.header.messageClass)
    {
        case MessageClass::request:
            if (message.header.method == bindingMethod)
                processBindingRequest(std::move(message));
            else
                m_messageHandler->serve(MessageContext{
                    shared_from_this(), std::move(message), m_attrs});
            break;

        default:
            NX_ASSERT(false);  //not supported yet
    }

    // Message handler has closed connection.
    if (!socket())
        closeConnection(SystemError::noError);
}

void ServerConnection::processBindingRequest(Message message)
{
    Message response(stun::Header(
        MessageClass::successResponse,
        bindingMethod, std::move(message.header.transactionId)));

    response.newAttribute< stun::attrs::XorMappedAddress >(
        m_peerAddress.port, ntohl(m_peerAddress.address.ipV4()->s_addr));

    sendMessage(std::move(response), nullptr);
}

} // namespace nx::network::stun
