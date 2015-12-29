/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#pragma once

#include <deque>
#include <functional>
#include <memory>

#include <nx/utils/log/log.h>
#include <utils/common/stoppable.h>
#include <utils/common/systemerror.h>

#include "nx/network/socket_common.h"


namespace nx {
namespace network {

/** Pipeline for transferring messages over unreliable transport (datagram socket).
    \a CustomPipeline MUST inherit this class and implement following methods:
    \code {*.cpp}
        //Received message from remote host
        void messageReceived(SocketAddress sourceAddress, MessageType mesage);
        //Unrecoverable error with socket
        void ioFailure(SystemError::ErrorCode);
    \endcode
 */
template<
    class CustomPipeline,
    class MessageType,
    class ParserType,
    class SerializerType>
class UnreliableMessagePipeline
:
    public QnStoppableAsync
{
    typedef UnreliableMessagePipeline<
        CustomPipeline,
        MessageType,
        ParserType,
        SerializerType
    > self_type;

public:
    UnreliableMessagePipeline(SocketFactory::NatTraversalType natTraversalType)
    :
        m_socket(SocketFactory::createDatagramSocket(natTraversalType))
    {
    }

    virtual void pleaseStop(std::function<void()> completionHandler) override
    {
        m_socket->pleaseStop(std::move(completionHandler));
    }

    bool bind(const SocketAddress& localAddress)
    {
        return m_socket->bind(localAddress);
    }

    /** Starts receving messages */
    void start()
    {
        using namespace std::placeholders;
        m_readBuffer.resize(0);
        m_readBuffer.reserve(nx::network::kMaxUDPDatagramSize);
        m_socket->recvFromAsync(
            &m_readBuffer, std::bind(&self_type::onBytesRead, this, _1, _2, _3));
    }

    SocketAddress address() const
    {
        return m_socket->getLocalAddress();
    }

    /** Messages are pipelined. I.e. this method can be called before previous message has been sent */
    void sendMessage(
        SocketAddress destinationEndpoint,
        MessageType message,
        std::function<void(SystemError::ErrorCode)> completionHandler)
    {
        m_socket->dispatch(std::bind(
            &self_type::sendMessageInternal,
            this,
            std::move(destinationEndpoint),
            std::move(message),
            std::move(completionHandler)));
    }

private:
    struct OutgoingMessageContext
    {
        SocketAddress destinationEndpoint;
        MessageType message;
        std::function<void(SystemError::ErrorCode)> completionHandler;
    };

    std::unique_ptr<AbstractDatagramSocket> m_socket;
    nx::Buffer m_readBuffer;
    nx::Buffer m_writeBuffer;
    ParserType m_messageParser;
    SerializerType m_messageSerializer;
    std::deque<OutgoingMessageContext> m_sendQueue;

    void onBytesRead(
        SystemError::ErrorCode errorCode,
        SocketAddress sourceAddress,
        size_t bytesRead)
    {
        static const std::chrono::seconds kRetryReadAfterFailureTimeout(1);

        if (errorCode != SystemError::noError)
        {
            NX_LOGX(lm("Error reading from socket. %1").
                arg(SystemError::toString(errorCode)), cl_logDEBUG1);
            static_cast<CustomPipeline*>(this)->ioFailure(errorCode);
            m_socket->registerTimer(
                kRetryReadAfterFailureTimeout,
                [this]() { start(); });
            return;
        }

        //reading and parsing message
        size_t bytesParsed = 0;
        MessageType msg;
        m_messageParser.setMessage(&msg);
        if (m_messageParser.parse(m_readBuffer, &bytesParsed) == nx_api::ParserState::done)
        {
            static_cast<CustomPipeline*>(this)->messageReceived(
                std::move(sourceAddress),
                std::move(msg));
        }
        else
        {
            NX_LOGX(lm("Failed to parse UDP datagram of size %1").
                arg((unsigned int)bytesRead), cl_logDEBUG1);
        }

        m_readBuffer.resize(0);
        m_readBuffer.reserve(nx::network::kMaxUDPDatagramSize);
        using namespace std::placeholders;
        m_socket->recvFromAsync(
            &m_readBuffer, std::bind(&self_type::onBytesRead, this, _1, _2, _3));
    }

    void sendMessageInternal(
        SocketAddress destinationEndpoint,
        MessageType message,
        std::function<void(SystemError::ErrorCode)> completionHandler)
    {
        OutgoingMessageContext msgCtx = {
            destinationEndpoint,
            std::move(message),
            std::move(completionHandler) };

        m_sendQueue.emplace_back(std::move(msgCtx));
        if (m_sendQueue.size() == 1)
            sendOutNextMessage();
    }

    void sendOutNextMessage()
    {
        OutgoingMessageContext& msgCtx = m_sendQueue.front();

        m_writeBuffer.resize(0);
        m_writeBuffer.reserve(1);
        m_messageSerializer.setMessage(&msgCtx.message);
        size_t bytesWritten = 0;
        assert(m_messageSerializer.serialize(&m_writeBuffer, &bytesWritten) ==
            nx_api::SerializerState::done);
        using namespace std::placeholders;
        m_socket->sendToAsync(
            m_writeBuffer,
            msgCtx.destinationEndpoint,
            std::bind(&self_type::messageSent, this, _1, _2));
    }

    void messageSent(SystemError::ErrorCode errorCode, size_t /*bytesSent*/)
    {
        assert(!m_sendQueue.empty());
        if (errorCode != SystemError::noError)
        {
            NX_LOGX(lm("Failed to send message destinationEndpoint %1. %2").
                arg(m_sendQueue.front().destinationEndpoint.toString()).
                arg(SystemError::toString(errorCode)), cl_logDEBUG1);
        }

        if (m_sendQueue.front().completionHandler)
            m_sendQueue.front().completionHandler(errorCode);
        m_sendQueue.pop_front();

        if (!m_sendQueue.empty())
            sendOutNextMessage();
    }
};

}   //network
}   //nx
