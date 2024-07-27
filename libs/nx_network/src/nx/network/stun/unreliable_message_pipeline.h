// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <functional>
#include <memory>

#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/connection_server/base_protocol_message_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/utils/system_error.h>

namespace nx::network {

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

    const std::unique_ptr<AbstractDatagramSocket>& socket();
    /**
     * Move ownership of socket to the caller.
     * UnreliableMessagePipeline is in undefined state after this call and MUST be freed immediately!
     * NOTE: Can be called within send/recv completion handler only
     *     (more specifically, within socket's aio thread)!
     */
    std::unique_ptr<AbstractDatagramSocket> takeSocket();

    void sendDatagram(
        SocketAddress destinationEndpoint,
        nx::Buffer datagram,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, SocketAddress)> completionHandler);

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
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            SocketAddress resolvedTargetAddress)> completionHandler;

        OutgoingMessageContext(
            SocketAddress _destinationEndpoint,
            nx::Buffer _serializedMessage,
            nx::utils::MoveOnlyFunc<void(
                SystemError::ErrorCode, SocketAddress)> _completionHandler);
        OutgoingMessageContext(OutgoingMessageContext&& rhs);

    private:
        OutgoingMessageContext(const OutgoingMessageContext&);
        OutgoingMessageContext& operator=(const OutgoingMessageContext&);
    };

    std::unique_ptr<AbstractDatagramSocket> m_socket;
    nx::Buffer m_readBuffer;
    std::deque<OutgoingMessageContext> m_sendQueue;
    nx::utils::InterruptionFlag m_terminationFlag;

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
 * This is a helper class for implementing UDP server/client of message-oriented protocol.
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
        nx::utils::MoveOnlyFunc<void(
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

    void setParserFactory(std::function<ParserType()> func)
    {
        m_parserFactory = std::move(func);
    }

private:
    CustomPipeline* m_customPipeline;
    std::function<ParserType()> m_parserFactory;

    virtual void datagramReceived(
        const SocketAddress& sourceAddress,
        const nx::Buffer& datagram) override
    {
        //reading and parsing message
        size_t bytesParsed = 0;
        MessageType msg;
        auto messageParser = m_parserFactory ? m_parserFactory() : ParserType{};
        messageParser.setMessage(&msg);
        if (messageParser.parse(datagram, &bytesParsed) == nx::network::server::ParserState::done)
        {
            m_customPipeline->messageReceived(sourceAddress, msg);
        }
        else
        {
            NX_INFO(this, "Failed to parse UDP datagram of size %1 received from %2 on %3. %4",
                datagram.size(), sourceAddress, address(), nx::utils::toBase64(datagram));
        }
    }

    virtual void ioFailure(SystemError::ErrorCode sysErrorCode) override
    {
        m_customPipeline->ioFailure(sysErrorCode);
    }
};

} // namespace nx::network
