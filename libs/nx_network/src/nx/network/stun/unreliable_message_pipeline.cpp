// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unreliable_message_pipeline.h"

#include "nx/network/socket_factory.h"

namespace nx::network {

DatagramPipeline::DatagramPipeline():
    m_socket(SocketFactory::createDatagramSocket())
{
    // TODO: #akolesnikov Should report this error to the caller.
    NX_ASSERT(m_socket->setNonBlockingMode(true));

    bindToAioThread(getAioThread());
}

DatagramPipeline::~DatagramPipeline()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void DatagramPipeline::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    BaseType::bindToAioThread(aioThread);

    m_socket->bindToAioThread(aioThread);
}

bool DatagramPipeline::bind(const SocketAddress& localAddress)
{
    return m_socket->bind(localAddress);
}

SocketAddress DatagramPipeline::address() const
{
    return m_socket->getLocalAddress();
}

void DatagramPipeline::startReceivingMessages()
{
    NX_VERBOSE(this, nx::format("startReceivingMessages. fd %1. local address %2")
        .arg(m_socket->handle()).arg(m_socket->getLocalAddress()));

    using namespace std::placeholders;
    m_readBuffer.resize(0);
    m_readBuffer.reserve(nx::network::kMaxUDPDatagramSize);
    m_socket->recvFromAsync(
        &m_readBuffer, std::bind(&DatagramPipeline::onBytesRead, this, _1, _2, _3));
}

void DatagramPipeline::stopReceivingMessagesSync()
{
    m_socket->cancelIOSync(aio::etRead);
}

const std::unique_ptr<AbstractDatagramSocket>& DatagramPipeline::socket()
{
    return m_socket;
}

std::unique_ptr<AbstractDatagramSocket> DatagramPipeline::takeSocket()
{
    m_terminationFlag.interrupt();
    m_socket->cancelIOSync(aio::etNone);
    return std::move(m_socket);
}

void DatagramPipeline::sendDatagram(
    SocketAddress destinationEndpoint,
    nx::Buffer datagram,
    utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress)> completionHandler)
{
    dispatch(
        [this, destinationEndpoint = std::move(destinationEndpoint),
            datagram = std::move(datagram),
            completionHandler = std::move(completionHandler)]() mutable
        {
            OutgoingMessageContext msgCtx(
                std::move(destinationEndpoint),
                std::move(datagram),
                std::move(completionHandler));

            m_sendQueue.push_back(std::move(msgCtx));
            if (m_sendQueue.size() == 1)
                sendOutNextMessage();
        });
}

void DatagramPipeline::stopWhileInAioThread()
{
    m_socket.reset();
}

void DatagramPipeline::onBytesRead(
    SystemError::ErrorCode errorCode,
    SocketAddress sourceAddress,
    size_t bytesRead)
{
    static const std::chrono::seconds kRetryReadAfterFailureTimeout(1);

    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, nx::format("Error reading from socket. %1")
            .arg(SystemError::toString(errorCode)));

        nx::utils::InterruptionFlag::Watcher watcher(&m_terminationFlag);
        ioFailure(errorCode);
        if (watcher.interrupted())
            return; //this has been freed
        m_socket->registerTimer(
            kRetryReadAfterFailureTimeout,
            [this]() { startReceivingMessages(); });
        return;
    }

    if (bytesRead > 0)  //zero-sized UDP datagramm is OK
    {
        nx::utils::InterruptionFlag::Watcher watcher(&m_terminationFlag);
        datagramReceived(sourceAddress, m_readBuffer);
        if (watcher.interrupted())
            return; //this has been freed
    }

    startReceivingMessages();
}

void DatagramPipeline::sendOutNextMessage()
{
    OutgoingMessageContext& msgCtx = m_sendQueue.front();

    using namespace std::placeholders;
    m_socket->sendToAsync(
        &msgCtx.serializedMessage,
        msgCtx.destinationEndpoint,
        std::bind(&DatagramPipeline::messageSent, this, _1, _2, _3));
}

void DatagramPipeline::messageSent(
    SystemError::ErrorCode errorCode,
    SocketAddress resolvedTargetAddress,
    size_t bytesSent)
{
    NX_ASSERT(!m_sendQueue.empty());
    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, nx::format("Failed to send message destinationEndpoint %1. %2").
            arg(m_sendQueue.front().destinationEndpoint.toString()).
            arg(SystemError::toString(errorCode)));
    }
    else
    {
        NX_ASSERT(bytesSent == (size_t)m_sendQueue.front().serializedMessage.size());
    }

    auto completionHandler = std::move(m_sendQueue.front().completionHandler);
    if (completionHandler)
    {
        nx::utils::InterruptionFlag::Watcher watcher(&m_terminationFlag);
        completionHandler(
            errorCode,
            std::move(resolvedTargetAddress));
        if (watcher.interrupted())
            return;
    }
    m_sendQueue.pop_front();

    if (!m_sendQueue.empty())
        sendOutNextMessage();
}

//-------------------------------------------------------------------------------------------------
// DatagramPipeline::OutgoingMessageContext

DatagramPipeline::OutgoingMessageContext::OutgoingMessageContext(
    SocketAddress _destinationEndpoint,
    nx::Buffer _serializedMessage,
    utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress)> _completionHandler)
    :
    destinationEndpoint(std::move(_destinationEndpoint)),
    serializedMessage(std::move(_serializedMessage)),
    completionHandler(std::move(_completionHandler))
{
}

DatagramPipeline::OutgoingMessageContext::OutgoingMessageContext(
    DatagramPipeline::OutgoingMessageContext&& rhs)
    :
    destinationEndpoint(std::move(rhs.destinationEndpoint)),
    serializedMessage(std::move(rhs.serializedMessage)),
    completionHandler(std::move(rhs.completionHandler))
{
}

} // namespace nx::network
