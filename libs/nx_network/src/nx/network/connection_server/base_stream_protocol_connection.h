// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <optional>

#include <nx/network/buffered_stream_socket.h>
#include <nx/utils/interruption_flag.h>
#include <nx/utils/qnbytearrayref.h>

#include "base_protocol_message_types.h"
#include "base_server_connection.h"
#include "detail/connection_statistics.h"

namespace nx::network::server {

/**
 * Connection of stream-orientied protocol of type request/respose.
 * It is not tied to underlying transport (tcp, udp, etc...).
 *
 * CustomConnection MUST implement following methods:
 * <pre><code>
 *     // Called for every received message.
 *     void processMessage(Message request);
 * </code></pre>
 *
 * NOTE: Serializer::serialize is allowed to reallocate source buffer if needed,
 *   but it is not recommended due to performance considerations!
 * NOTE: It is allowed to free instance within event handler.
 */
template<
    typename CustomConnection,
    typename Message,
    typename Parser,
    typename Serializer
> class BaseStreamProtocolConnection:
    public BaseServerConnection
{
    using self_type = BaseStreamProtocolConnection<
        CustomConnection,
        Message,
        Parser,
        Serializer>;

    using base_type = BaseServerConnection;

public:
    using MessageType = Message;

    detail::ConnectionStatistics connectionStatistics;

    BaseStreamProtocolConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(std::move(streamSocket)),
        m_creationTimestamp(std::chrono::steady_clock::now())
    {
        static constexpr size_t kDefaultSendBufferSize = 4 * 1024;
        m_writeBuffer.reserve(kDefaultSendBufferSize);

        m_parser.setMessage(&m_message);

        base_type::registerCloseHandler(
            [this](auto errorCode, auto /*connectionDestroyed*/)
            {
                onConnectionClosed(errorCode);
            });
    }

    virtual ~BaseStreamProtocolConnection() = default;

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override
    {
        auto streamSocket = base_type::takeSocket();
        if (!m_dataToParse.empty())
        {
            auto bufferedSocket = std::make_unique<nx::network::BufferedStreamSocket>(
                std::move(streamSocket),
                nx::Buffer(m_dataToParse));
            streamSocket = std::move(bufferedSocket);
        }

        return streamSocket;
    }

    /**
     * Initiates asynchronous message send.
     */
    void sendMessage(
        Message msg,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        auto newTask = std::make_unique<SendTask>(
            std::move(msg),
            std::move(handler));
        addNewTaskToQueue(std::move(newTask));
    }

    void sendMessage(Message msg) //< Template cannot be resolved by default value.
    {
        sendMessage(std::move(msg), nullptr);
    }

    void sendData(
        nx::Buffer data,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        auto newTask = std::make_unique<SendTask>(
            std::move(data),
            std::move(handler));
        addNewTaskToQueue(std::move(newTask));
    }

    /**
     * Implements the same behavior as AbstractMsgBodySource::sendAsync.
     */
    void sendAsync(const nx::Buffer* data, IoCompletionHandler handler)
    {
        auto newTask = std::make_unique<SendTask>(
            data,
            [bytesToSend = data->size(), handler = std::move(handler)](
                auto resultCode)
            {
                handler(
                    resultCode,
                    resultCode == SystemError::noError ? bytesToSend : -1);
            });
        addNewTaskToQueue(std::move(newTask));
    }

    const Parser& parser() const { return m_parser; }
    Parser& parser() { return m_parser; }

    const Serializer& serializer() const { return m_serializer; }
    Serializer& serializer() { return m_serializer; }

    virtual void cancelWrite() override
    {
        this->executeInAioThreadSync(
            [this]()
            {
                base_type::cancelWrite();
                m_sendQueue.clear();
            });
    }

protected:
    virtual void processMessage(Message message) = 0;
    virtual void processSomeMessageBody(nx::Buffer /*messageBodyBuffer*/) {}
    virtual void processMessageEnd() {}

    virtual void readyToSendData() override
    {
        // Using clear will clear the reserved buffer in QByteArray.
        m_writeBuffer.resize(0);

        if (m_serializerState == SerializerState::done)
        {
            // Message is sent, triggerring completion handler.
            if (!completeCurrentSendTask())
                return;
            processAnotherSendTaskIfAny();
        }
        else if (m_serializerState == SerializerState::needMoreBufferSpace)
        {
            serializeMessage();

            // Assuming that all bytes will be written or none.
            base_type::sendBufAsync(&m_writeBuffer);
        }
        else
        {
            NX_ASSERT(false, nx::format("Unknown serializer state: %1")
                .args(static_cast<int>(m_serializerState)));
        }
    }

    virtual void bytesReceived(const nx::Buffer& buf) override
    {
        m_dataToParse = buf;

        do
        {
            // If m_dataToParse is empty we report end of file to the parser.
            if (!invokeMessageParser())
                return; //< TODO: #akolesnikov Ignore all following data and close the connection?
        }
        while (!m_dataToParse.empty());

        m_dataToParse = {};
    }

private:
    using SendHandler = nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

    struct SendTask
    {
    public:
        SendTask() = default;

        SendTask(Message msg, SendHandler handler):
            msg(std::move(msg)),
            handler(std::move(handler))
        {
        }

        SendTask(nx::Buffer buf, SendHandler handler):
            buf(std::move(buf)),
            handler(std::move(handler))
        {
        }

        SendTask(const nx::Buffer* userBuf, SendHandler handler):
            userBuf(userBuf),
            handler(std::move(handler))
        {
        }

        SendTask(SendTask&& /*right*/) = default;

        SendTask(const SendTask&) = delete;
        SendTask& operator=(const SendTask&) = delete;

        std::optional<Message> msg;
        std::optional<nx::Buffer> buf;
        std::optional<const nx::Buffer*> userBuf;
        SendHandler handler;
        bool asyncSendIssued = false;
    };

    Message m_message;
    Parser m_parser;
    Serializer m_serializer;
    SerializerState m_serializerState = SerializerState::done;
    nx::Buffer m_writeBuffer;
    std::deque<SendTask> m_sendQueue;
    nx::utils::InterruptionFlag m_connectionFreedFlag;
    bool m_messageReported = false;
    nx::ConstBufferRefType m_dataToParse;
    std::chrono::steady_clock::time_point m_creationTimestamp;

    /**
     * @param buf Source buffer.
     * @param pos Position inside source buffer. Moved by number of bytes read.
     * @return false in case of parsing is stopped and cannot be resumed.
     *   This method should not be called anymore since parser can be in undefined state.
     */
    bool invokeMessageParser()
    {
        size_t bytesProcessed = 0;
        const auto parserState = m_parser.parse(m_dataToParse, &bytesProcessed);
        m_dataToParse.remove_prefix(bytesProcessed);
        switch (parserState)
        {
            case ParserState::init:
            case ParserState::readingMessage:
                NX_ASSERT(m_dataToParse.empty());
                break;

            case ParserState::readingBody:
            {
                if (!reportMessageIfNeeded())
                    return false;
                if (!reportMsgBodyIfHaveSome())
                    return false;
                break;
            }

            case ParserState::done:
            {
                connectionStatistics.messageReceived();

                if (!reportMessageIfNeeded())
                    return false;
                if (!reportMsgBodyIfHaveSome())
                    return false;
                if (!reportMessageEnd())
                    return false;

                resetParserState();
                break;
            }

            case ParserState::failed:
                // TODO: #akolesnikov Ignore all further data and close connection?
                return false;
        }

        return true;
    }

    bool reportMessageIfNeeded()
    {
        if (m_messageReported)
            return true;

        nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
        processMessage(std::exchange(m_message, Message()));
        if (watcher.interrupted())
            return false; //< Connection has been removed by handler.

        m_messageReported = true;
        return true;
    }

    bool reportMsgBodyIfHaveSome()
    {
        auto msgBodyBuffer = m_parser.fetchMessageBody();
        if (msgBodyBuffer.empty())
            return true;

        nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
        processSomeMessageBody(std::move(msgBodyBuffer));
        if (watcher.interrupted())
            return false; //< Connection has been removed by handler.

        return true;
    }

    bool reportMessageEnd()
    {
        nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
        processMessageEnd();
        return !watcher.interrupted();
    }

    void resetParserState()
    {
        m_parser.reset();
        m_message = Message();
        m_parser.setMessage(&m_message);
        m_messageReported = false;
    }

    void sendMessageInternal(const Message& msg)
    {
        // Serializing message.
        m_serializer.setMessage(&msg);
        m_serializerState = SerializerState::needMoreBufferSpace;
        readyToSendData();
    }

    void addNewTaskToQueue(std::unique_ptr<SendTask> newTask)
    {
        this->dispatch(
            [this, newTask = std::move(newTask)]()
            {
                m_sendQueue.push_back(std::move(*newTask));
                if (m_sendQueue.size() > 1)
                    return; //< Already processing another message.

                processAnotherSendTaskIfAny();
            });
    }

    /**
     * @return false If was interrupted. All futher processing should be stopped until the next event.
     */
    bool completeCurrentSendTask()
    {
        NX_ASSERT(!m_sendQueue.empty());
        // NOTE: Completion handler is allowed to delete this connection object.
        auto sendCompletionHandler =
            std::exchange(m_sendQueue.front().handler, nullptr);
        m_serializer.setMessage(nullptr);
        m_sendQueue.pop_front();

        if (sendCompletionHandler)
        {
            nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
            sendCompletionHandler(SystemError::noError);
            return !watcher.interrupted();
        }

        return true;
    }

    void processAnotherSendTaskIfAny()
    {
        // Sending next message in queue (if any).
        if (m_sendQueue.empty() || m_sendQueue.front().asyncSendIssued)
            return;

        if (!socket())
            return completeSendTasksWithError(SystemError::notConnected);

        auto& task = m_sendQueue.front();
        task.asyncSendIssued = true;
        if (task.msg)
        {
            sendMessageInternal(*task.msg);
        }
        else if (task.buf)
        {
            NX_ASSERT(m_writeBuffer.empty());
            m_writeBuffer = std::exchange(*task.buf, {});
            m_serializerState = SerializerState::done;
            base_type::sendBufAsync(&m_writeBuffer);
        }
        else if (task.userBuf)
        {
            NX_ASSERT(m_writeBuffer.empty());
            m_serializerState = SerializerState::done;
            base_type::sendBufAsync(*task.userBuf);
        }
    }

    void serializeMessage()
    {
        size_t bytesWritten = 0;
        m_serializerState = m_serializer.serialize(&m_writeBuffer, &bytesWritten);
        if ((m_serializerState == SerializerState::needMoreBufferSpace) &&
            (bytesWritten == 0))
        {
            // TODO: #akolesnikov Increase buffer.
            NX_ASSERT(false);
        }
    }

    void onConnectionClosed(SystemError::ErrorCode errorCode)
    {
        completeSendTasksWithError(errorCode);
    }

    void completeSendTasksWithError(SystemError::ErrorCode errorCode)
    {
        // NOTE: to avoid a deadloop (when user schedules new send tasks within the handler)
        // not processing tasks scheduled within the handler.

        const auto tasksToProcessCount = m_sendQueue.size();
        for (std::size_t i = 0; i < tasksToProcessCount; ++i)
        {
            auto handler = std::exchange(m_sendQueue.front().handler, nullptr);
            if (m_sendQueue.front().asyncSendIssued)
                m_writeBuffer.resize(0); //< The send failed, disregarding the data.

            if (handler)
            {
                nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
                handler(errorCode);
                if (watcher.interrupted())
                    return; //< Connection has been removed by handler.
            }

            if (m_sendQueue.size() < tasksToProcessCount - i)
                break; // Send has been cancelled.

            m_sendQueue.pop_front();
        }
    }
};

/**
 * Inherits BaseStreamProtocolConnection and delegates processMessage to the functor
 * set with StreamProtocolConnection::setMessageHandler.
 */
template<
    class Message,
    class Parser,
    class Serializer
> class StreamProtocolConnection:
    public BaseStreamProtocolConnection<
        StreamProtocolConnection<Message, Parser, Serializer>,
        Message,
        Parser,
        Serializer>
{
    using self_type = StreamProtocolConnection<
        Message,
        Parser,
        Serializer>;

    using base_type = BaseStreamProtocolConnection<
        self_type,
        Message,
        Parser,
        Serializer>;

public:
    using OnConnectionClosedHandler =
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode /*closeReason*/)>;

    StreamProtocolConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(std::move(streamSocket))
    {
    }

    /**
     * @param handler Receives all incoming messages.
     * NOTE: It is required to call BaseStreamProtocolConnection::startReadingConnection
     *   to start receiving messages.
     */
    template<class T>
    void setMessageHandler(T&& handler)
    {
        m_messageHandler = std::forward<T>(handler);
    }

    template<class T>
    void setOnSomeMessageBodyAvailable(T&& handler)
    {
        m_messageBodyHandler = std::forward<T>(handler);
    }

    template<class T>
    void setOnMessageEnd(T&& handler)
    {
        m_messageEndHandler = std::forward<T>(handler);
    }

protected:
    virtual void processMessage(Message msg) override
    {
        if (m_messageHandler)
            m_messageHandler(std::move(msg));
    }

    virtual void processSomeMessageBody(nx::Buffer messageBodyBuffer) override
    {
        if (m_messageBodyHandler)
            m_messageBodyHandler(std::move(messageBodyBuffer));
    }

    virtual void processMessageEnd() override
    {
        if (m_messageEndHandler)
            m_messageEndHandler();
    }

private:
    std::function<void(Message)> m_messageHandler;
    std::function<void(nx::Buffer)> m_messageBodyHandler;
    std::function<void()> m_messageEndHandler;
};

} // namespace nx::network::server
