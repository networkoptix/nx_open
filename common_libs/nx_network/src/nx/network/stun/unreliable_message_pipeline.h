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
#include "nx/network/socket_factory.h"


namespace nx {
namespace network {

/** Pipeline for transferring messages over unreliable transport (datagram socket).
    \a CustomPipeline MUST implement following methods:
    \code {*.cpp}
        //Received message from remote host
        void messageReceived(SocketAddress sourceAddress, MessageType mesage);
        //Unrecoverable error with socket
        void ioFailure(SystemError::ErrorCode);
    \endcode
    All these methods are invoked within socket thread
    \note This is helper class for implementing various UDP server/client
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
    UnreliableMessagePipeline(CustomPipeline* customPipeline)
    :
        m_customPipeline(customPipeline),
        m_socket(SocketFactory::createDatagramSocket())
    {
    }

    /**
        \note \a completionHandler is invoked in socket's aio thread
    */
    virtual void pleaseStop(std::function<void()> completionHandler) override
    {
        m_socket->pleaseStop(std::move(completionHandler));
    }

    /** If not called, any vacant local port will be used */
    bool bind(const SocketAddress& localAddress)
    {
        return m_socket->bind(localAddress);
    }

    void startReceivingMessages()
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
        const MessageType& message,
        std::function<void(SystemError::ErrorCode)> completionHandler)
    {
        //serializing message
        SerializerType messageSerializer;
        nx::Buffer serializedMessage;
        serializedMessage.reserve(nx::network::kTypicalMtuSize);
        messageSerializer.setMessage(&message);
        size_t bytesWritten = 0;
        assert(messageSerializer.serialize(&serializedMessage, &bytesWritten) ==
                nx_api::SerializerState::done);

        m_socket->dispatch(std::bind(
            &self_type::sendMessageInternal,
            this,
            std::move(destinationEndpoint),
            std::move(serializedMessage),
            std::move(completionHandler)));
    }

    const std::unique_ptr<AbstractDatagramSocket>& socket() const
    {
        return m_socket;
    }

private:
    struct OutgoingMessageContext
    {
        SocketAddress destinationEndpoint;
        nx::Buffer serializedMessage;
        std::function<void(SystemError::ErrorCode)> completionHandler;

        OutgoingMessageContext(
            SocketAddress _destinationEndpoint,
            nx::Buffer _serializedMessage,
            std::function<void(SystemError::ErrorCode)> _completionHandler)
        :
            destinationEndpoint(std::move(_destinationEndpoint)),
            serializedMessage(std::move(_serializedMessage)),
            completionHandler(std::move(_completionHandler))
        {
        }

        OutgoingMessageContext(OutgoingMessageContext&& rhs)
        :
            destinationEndpoint(std::move(rhs.destinationEndpoint)),
            serializedMessage(std::move(rhs.serializedMessage)),
            completionHandler(std::move(rhs.completionHandler))
        {
        }

    private:
        OutgoingMessageContext(const OutgoingMessageContext&);
        OutgoingMessageContext& operator=(const OutgoingMessageContext&);
    };

    CustomPipeline* m_customPipeline;
    std::unique_ptr<AbstractDatagramSocket> m_socket;
    nx::Buffer m_readBuffer;
    ParserType m_messageParser;
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
            m_customPipeline->ioFailure(errorCode);
            m_socket->registerTimer(
                kRetryReadAfterFailureTimeout,
                [this]() { startReceivingMessages(); });
            return;
        }

        //reading and parsing message
        size_t bytesParsed = 0;
        MessageType msg;
        m_messageParser.setMessage(&msg);
        if (m_messageParser.parse(m_readBuffer, &bytesParsed) == nx_api::ParserState::done)
        {
            m_customPipeline->messageReceived(
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
        nx::Buffer serializedMessage,
        std::function<void(SystemError::ErrorCode)> completionHandler)
    {
        OutgoingMessageContext msgCtx(
            std::move(destinationEndpoint),
            std::move(serializedMessage),
            std::move(completionHandler));

        m_sendQueue.emplace_back(std::move(msgCtx));
        if (m_sendQueue.size() == 1)
            sendOutNextMessage();
    }

    void sendOutNextMessage()
    {
        OutgoingMessageContext& msgCtx = m_sendQueue.front();

        using namespace std::placeholders;
        m_socket->sendToAsync(
            msgCtx.serializedMessage,
            msgCtx.destinationEndpoint,
            std::bind(&self_type::messageSent, this, _1, _2));
    }

    void messageSent(SystemError::ErrorCode errorCode, size_t bytesSent)
    {
        assert(!m_sendQueue.empty());
        if (errorCode != SystemError::noError)
        {
            NX_LOGX(lm("Failed to send message destinationEndpoint %1. %2").
                arg(m_sendQueue.front().destinationEndpoint.toString()).
                arg(SystemError::toString(errorCode)), cl_logDEBUG1);
        }
        else
        {
            assert(bytesSent == m_sendQueue.front().serializedMessage.size());
        }

        if (m_sendQueue.front().completionHandler)
            m_sendQueue.front().completionHandler(errorCode);
        m_sendQueue.pop_front();

        if (!m_sendQueue.empty())
            sendOutNextMessage();
    }

    UnreliableMessagePipeline(const UnreliableMessagePipeline&);
    UnreliableMessagePipeline& operator=(const UnreliableMessagePipeline&);
};

}   //network
}   //nx
