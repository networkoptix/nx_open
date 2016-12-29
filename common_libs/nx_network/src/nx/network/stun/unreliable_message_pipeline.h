/**********************************************************
* Dec 28, 2015
* akolesnikov
***********************************************************/

#pragma once

#include <deque>
#include <functional>
#include <memory>

#include <nx/utils/log/log.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/stoppable.h>
#include <utils/common/systemerror.h>

#include "nx/network/aio/basic_pollable.h"
#include "nx/network/socket_common.h"
#include "nx/network/socket_factory.h"
#include "nx/network/system_socket.h"

namespace nx {
namespace network {

/**
 * Implement this interface to receive messages and events from UnreliableMessagePipeline.
 */
template<class MessageType>
class UnreliableMessagePipelineEventHandler
{
public:
    virtual ~UnreliableMessagePipelineEventHandler() {}

    virtual void messageReceived(
        SocketAddress sourceAddress,
        MessageType msg) = 0;
    virtual void ioFailure(SystemError::ErrorCode errorCode) = 0;
};

/**
 * Pipeline for transferring messages over unreliable transport (datagram socket).
 * This is a helper class for implementing UDP server/client of message-orientied protocol.
 * Received messages are delivered to UnreliableMessagePipelineEventHandler<MessageType> instance.
 * @note All events are delivered within internal socket's aio thread
 * @note \a UnreliableMessagePipeline object can be safely freed within any event handler
 *     (more generally, within internal socket's aio thread).
       To delete it in another thread, cancel I/O with UnreliableMessagePipeline::pleaseStop call
 */
template<
    class MessageType,
    class ParserType,
    class SerializerType>
class UnreliableMessagePipeline:
    public aio::BasicPollable
{
    typedef UnreliableMessagePipeline<
        MessageType,
        ParserType,
        SerializerType
    > SelfType;

    using BaseType = aio::BasicPollable;

public:
    typedef UnreliableMessagePipelineEventHandler<MessageType> CustomPipeline;

    /**
     * @param customPipeline UnreliableMessagePipeline does not take ownership of customPipeline.
     */
    UnreliableMessagePipeline(CustomPipeline* customPipeline):
        m_customPipeline(customPipeline),
        m_socket(std::make_unique<UDPSocket>())
    {
        // TODO: #ak Should report this error to the caller.
        NX_ASSERT(m_socket->setNonBlockingMode(true));

        bindToAioThread(getAioThread());
    }

    virtual ~UnreliableMessagePipeline() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        BaseType::bindToAioThread(aioThread);

        m_socket->bindToAioThread(aioThread);
    }

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
            &m_readBuffer, std::bind(&SelfType::onBytesRead, this, _1, _2, _3));
    }

    SocketAddress address() const
    {
        return m_socket->getLocalAddress();
    }

    /**
     * Messages are pipelined. I.e. this method can be called before previous message has been sent.
     */
    void sendMessage(
        SocketAddress destinationEndpoint,
        const MessageType& message,
        utils::MoveOnlyFunc<void(
            SystemError::ErrorCode /*sysErrorCode*/,
            SocketAddress /*resolvedTargetAddress*/)> completionHandler)
    {
        //serializing message
        SerializerType messageSerializer;
        nx::Buffer serializedMessage;
        serializedMessage.reserve(nx::network::kTypicalMtuSize);
        messageSerializer.setMessage(&message);
        size_t bytesWritten = 0;
        if (messageSerializer.serialize(&serializedMessage, &bytesWritten) != 
            nx_api::SerializerState::done)
        {
            NX_ASSERT(false);
        }

        m_socket->dispatch(
            [this, endpoint = std::move(destinationEndpoint),
                   message = std::move(serializedMessage),
                   handler = std::move(completionHandler)]() mutable
        {
            sendMessageInternal(
                std::move(endpoint), std::move(message), std::move(handler));
        });
    }

    const std::unique_ptr<network::UDPSocket>& socket()
    {
        return m_socket;
    }

    /**
     * Move ownership of socket to the caller.
     * UnreliableMessagePipeline is in undefined state after this call and MUST be freed immediately!
     * @note Can be called within send/recv completion handler only
     *     (more specifically, within socket's aio thread)!
     */
    std::unique_ptr<network::UDPSocket> takeSocket()
    {
        m_terminationFlag.markAsDeleted();
        m_socket->cancelIOSync(aio::etNone); 
        return std::move(m_socket);
    }

private:
    struct OutgoingMessageContext
    {
        SocketAddress destinationEndpoint;
        nx::Buffer serializedMessage;
        utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            SocketAddress resolvedTargetAddress)> completionHandler;

        OutgoingMessageContext(
            SocketAddress _destinationEndpoint,
            nx::Buffer _serializedMessage,
            utils::MoveOnlyFunc<void(
                SystemError::ErrorCode, SocketAddress)> _completionHandler)
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
    std::unique_ptr<UDPSocket> m_socket;
    nx::Buffer m_readBuffer;
    ParserType m_messageParser;
    std::deque<OutgoingMessageContext> m_sendQueue;
    nx::utils::ObjectDestructionFlag m_terminationFlag;

    virtual void stopWhileInAioThread() override
    {
        m_socket.reset();
    }

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

            nx::utils::ObjectDestructionFlag::Watcher watcher(&m_terminationFlag);
            m_customPipeline->ioFailure(errorCode);
            if (watcher.objectDestroyed())
                return; //this has been freed
            m_socket->registerTimer(
                kRetryReadAfterFailureTimeout,
                [this]() { startReceivingMessages(); });
            return;
        }

        if (bytesRead > 0)  //zero-sized UDP datagramm is OK
        {
            //reading and parsing message
            size_t bytesParsed = 0;
            MessageType msg;
            m_messageParser.setMessage(&msg);
            if (m_messageParser.parse(m_readBuffer, &bytesParsed) == nx_api::ParserState::done)
            {
                nx::utils::ObjectDestructionFlag::Watcher watcher(&m_terminationFlag);
                m_customPipeline->messageReceived(
                    std::move(sourceAddress),
                    std::move(msg));
                if (watcher.objectDestroyed())
                    return; //this has been freed
            }
            else
            {
                NX_LOGX(lm("Failed to parse UDP datagram of size %1").
                    arg((unsigned int)bytesRead), cl_logDEBUG1);
            }
        }

        m_readBuffer.resize(0);
        m_readBuffer.reserve(nx::network::kMaxUDPDatagramSize);
        using namespace std::placeholders;
        m_socket->recvFromAsync(
            &m_readBuffer, std::bind(&SelfType::onBytesRead, this, _1, _2, _3));
    }

    void sendMessageInternal(
        SocketAddress destinationEndpoint,
        nx::Buffer serializedMessage,
        utils::MoveOnlyFunc<void(
            SystemError::ErrorCode, SocketAddress)> completionHandler)
    {
        OutgoingMessageContext msgCtx(
            std::move(destinationEndpoint),
            std::move(serializedMessage),
            std::move(completionHandler));

        m_sendQueue.push_back(std::move(msgCtx));
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
            std::bind(&SelfType::messageSent, this, _1, _2, _3));
    }

    void messageSent(
        SystemError::ErrorCode errorCode,
        SocketAddress resolvedTargetAddress,
        size_t bytesSent)
    {
        NX_ASSERT(!m_sendQueue.empty());
        if (errorCode != SystemError::noError)
        {
            NX_LOGX(lm("Failed to send message destinationEndpoint %1. %2").
                arg(m_sendQueue.front().destinationEndpoint.toString()).
                arg(SystemError::toString(errorCode)), cl_logDEBUG1);
        }
        else
        {
            NX_ASSERT(bytesSent == (size_t)m_sendQueue.front().serializedMessage.size());
        }

        auto completionHandler = std::move(m_sendQueue.front().completionHandler);
        if (completionHandler)
        {
            nx::utils::ObjectDestructionFlag::Watcher watcher(&m_terminationFlag);
            completionHandler(
                errorCode,
                std::move(resolvedTargetAddress));
            if (watcher.objectDestroyed())
                return;
        }
        m_sendQueue.pop_front();

        if (!m_sendQueue.empty())
            sendOutNextMessage();
    }

    UnreliableMessagePipeline(const UnreliableMessagePipeline&);
    UnreliableMessagePipeline& operator=(const UnreliableMessagePipeline&);
};

} // namespace network
} // namespace nx
