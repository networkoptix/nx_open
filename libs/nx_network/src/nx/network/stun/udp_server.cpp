// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "udp_server.h"

#include <memory>

#include <nx/utils/log/log.h>

#include "abstract_server_connection.h"
#include "abstract_message_handler.h"
#include "udp_message_response_sender.h"

namespace nx {
namespace network {
namespace stun {

UdpServer::UdpServer(AbstractMessageHandler* messageHandler):
    m_messagePipeline(this),
    m_messageHandler(messageHandler),
    m_activeRequestCounter(std::make_shared<nx::utils::Counter>())
{
    bindToAioThread(getAioThread());

    m_messagePipeline.setParserFactory([this]() {
        stun::MessageParser parser;
        parser.setFingerprintRequired(m_fingerprintRequired);
        return parser;
    });
}

UdpServer::~UdpServer()
{
    pleaseStopSync();
}

void UdpServer::bindToAioThread(network::aio::AbstractAioThread* aioThread)
{
    network::aio::BasicPollable::bindToAioThread(aioThread);

    m_messagePipeline.bindToAioThread(aioThread);
}

bool UdpServer::bind(const SocketAddress& localAddress)
{
    m_boundToLocalAddress = true;
    return m_messagePipeline.bind(localAddress);
}

bool UdpServer::listen(int /*backlogSize*/)
{
    if (!m_boundToLocalAddress)
    {
        if (!bind(SocketAddress(HostAddress::anyHost, 0)))
            return false;
        m_boundToLocalAddress = true;
    }
    m_messagePipeline.startReceivingMessages();
    return true;
}

void UdpServer::stopReceivingMessagesSync()
{
    m_messagePipeline.stopReceivingMessagesSync();
}

SocketAddress UdpServer::address() const
{
    return m_messagePipeline.address();
}

void UdpServer::sendMessage(
    SocketAddress destinationEndpoint,
    const Message& message,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> completionHandler)
{
    m_messagePipeline.sendMessage(
        std::move(destinationEndpoint), message,
        [completionHandler = std::move(completionHandler)](
            SystemError::ErrorCode errorCode, SocketAddress) mutable
        {
            if (completionHandler)
                completionHandler(errorCode);
        });
}

const std::unique_ptr<AbstractDatagramSocket>& UdpServer::socket()
{
    return m_messagePipeline.socket();
}

void UdpServer::waitUntilAllRequestsCompleted()
{
    m_activeRequestCounter->wait();
}

void UdpServer::setFingerprintRequired(bool val)
{
    m_fingerprintRequired = val;
}

nx::network::server::Statistics UdpServer::statistics() const
{
    // TODO: #akolesnikov
    return nx::network::server::Statistics();
}

void UdpServer::stopWhileInAioThread()
{
    m_messagePipeline.pleaseStopSync();
}

void UdpServer::messageReceived(SocketAddress sourceAddress, Message message)
{
    m_messageHandler->serve(MessageContext{
        std::make_shared<UDPMessageResponseSender>(
            this, m_activeRequestCounter, std::move(sourceAddress)),
        std::move(message),
        {} /*empty attributes*/});
}

void UdpServer::ioFailure(SystemError::ErrorCode)
{
    //TODO #akolesnikov
}

} // namespace stun
} // namespace network
} // namespace nx
