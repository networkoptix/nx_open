#pragma once

#include <deque>
#include <functional>
#include <memory>

#include <nx/network/async_stoppable.h>
#include <nx/utils/log/log.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>

#include "nx/network/aio/basic_pollable.h"
#include "nx/network/connection_server/base_protocol_message_types.h"
#include "nx/network/socket_common.h"
#include "nx/network/socket_factory.h"
#include "nx/network/system_socket.h"

namespace nx {
namespace network {

class NX_NETWORK_API DatagramPipeline:
    public aio::BasicPollable
{
    using BaseType = aio::BasicPollable;

public:
    DatagramPipeline();
    virtual ~DatagramPipeline() override;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    bool bind(const SocketAddress& localAddress);
    SocketAddress address() const;

    void startReceivingMessages();
    void stopReceivingMessagesSync();

    const std::unique_ptr<network::UDPSocket>& socket();
    /**
     * Move ownership of socket to the caller.
     * UnreliableMessagePipeline is in undefined state after this call and MUST be freed immediately!
     * NOTE: Can be called within send/recv completion handler only
     *     (more specifically, within socket's aio thread)!
     */
    std::unique_ptr<network::UDPSocket> takeSocket();

    void sendDatagram(
        SocketAddress destinationEndpoint,
        nx::Buffer datagram,
        utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress)> completionHandler);

protected:
    virtual void datagramReceived(
        const SocketAddress& sourceAddress,
        const nx::Buffer& datagram) = 0;
    virtual void ioFailure(SystemError::ErrorCode sysErrorCode) = 0;

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
                SystemError::ErrorCode, SocketAddress)> _completionHandler);
        OutgoingMessageContext(OutgoingMessageContext&& rhs);

    private:
        OutgoingMessageContext(const OutgoingMessageContext&);
        OutgoingMessageContext& operator=(const OutgoingMessageContext&);
    };

    std::unique_ptr<UDPSocket> m_socket;
    nx::Buffer m_readBuffer;
    std::deque<OutgoingMessageContext> m_sendQueue;
    nx::utils::ObjectDestructionFlag m_terminationFlag;

    virtual void stopWhileInAioThread() override;

    void onBytesRead(
        SystemError::ErrorCode errorCode,
        SocketAddress sourceAddress,
        size_t bytesRead);

    void sendOutNextMessage();

    void messageSent(
        SystemError::ErrorCode errorCode,
        SocketAddress resolvedTargetAddress,
        size_t bytesSent);

    DatagramPipeline(const DatagramPipeline&);
    DatagramPipeline& operator=(const DatagramPipeline&);
};

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
 * NOTE: All events are delivered within internal socket's aio thread
 * NOTE: UnreliableMessagePipeline object can be safely freed within any event handler
 *     (more generally, within internal socket's aio thread).
       To delete it in another thread, cancel I/O with UnreliableMessagePipeline::pleaseStop call
 */
template<
    class MessageType,
    class ParserType,
    class SerializerType>
class UnreliableMessagePipeline:
    public DatagramPipeline
{
    typedef UnreliableMessagePipeline<
        MessageType,
        ParserType,
        SerializerType
    > SelfType;

    using BaseType = DatagramPipeline;

public:
    typedef UnreliableMessagePipelineEventHandler<MessageType> CustomPipeline;

    /**
     * @param customPipeline UnreliableMessagePipeline does not take ownership of customPipeline.
     */
    UnreliableMessagePipeline(CustomPipeline* customPipeline):
        m_customPipeline(customPipeline)
    {
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
            nx::network::server::SerializerState::done)
        {
            NX_ASSERT(false);
        }

        sendDatagram(
            std::move(destinationEndpoint),
            std::move(serializedMessage),
            std::move(completionHandler));
    }

private:
    CustomPipeline* m_customPipeline;
    ParserType m_messageParser;

    virtual void datagramReceived(
        const SocketAddress& sourceAddress,
        const nx::Buffer& datagram) override
    {
        //reading and parsing message
        size_t bytesParsed = 0;
        MessageType msg;
        m_messageParser.setMessage(&msg);
        if (m_messageParser.parse(datagram, &bytesParsed) == nx::network::server::ParserState::done)
        {
            m_customPipeline->messageReceived(
                std::move(sourceAddress),
                std::move(msg));
        }
        else
        {
            NX_LOGX(lm("Failed to parse UDP datagram of size %1 received from %2 on %3")
                .arg((unsigned int)datagram.size()).arg(sourceAddress).arg(address()),
                cl_logDEBUG1);
        }
    }

    virtual void ioFailure(SystemError::ErrorCode sysErrorCode) override
    {
        m_customPipeline->ioFailure(sysErrorCode);
    }
};

} // namespace network
} // namespace nx
