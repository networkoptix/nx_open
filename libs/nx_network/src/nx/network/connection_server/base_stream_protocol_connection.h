#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <optional>

#include <nx/network/buffered_stream_socket.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/qnbytearrayref.h>

#include "base_protocol_message_types.h"
#include "base_server_connection.h"

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
    public BaseServerConnection<CustomConnection>
{
    using self_type = BaseStreamProtocolConnection<
        CustomConnection,
        Message,
        Parser,
        Serializer>;
    using base_type = BaseServerConnection<CustomConnection>;

public:
    using MessageType = Message;

    BaseStreamProtocolConnection(
        std::unique_ptr<AbstractStreamSocket> streamSocket)
        :
        base_type(std::move(streamSocket)),
        m_creationTimestamp(std::chrono::steady_clock::now())
    {
        static constexpr size_t kDefaultSendBufferSize = 4 * 1024;
        m_writeBuffer.reserve(kDefaultSendBufferSize);

        m_parser.setMessage(&m_message);
    }

    virtual ~BaseStreamProtocolConnection() = default;

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override
    {
        auto streamSocket = base_type::takeSocket();
        if (!m_dataToParse.isEmpty())
        {
            auto bufferedSocket = std::make_unique<nx::network::BufferedStreamSocket>(
                std::move(streamSocket),
                m_dataToParse);
            streamSocket = std::move(bufferedSocket);
        }

        return streamSocket;
    }

    void bytesReceived(const nx::Buffer& buf)
    {
        m_dataToParse = buf;

        do
        {
            // If m_dataToParse is empty we report end of file to the parser.
            if (!invokeMessageParser())
                return; //< TODO: #ak Ignore all following data and close the connection?
        }
        while (!m_dataToParse.isEmpty());

        m_dataToParse.clear();
    }

    void readyToSendData()
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
            base_type::sendBufAsync(m_writeBuffer);
        }
        else
        {
            NX_ASSERT(false, lm("Unknown serializer state: %1")
                .args(static_cast<int>(m_serializerState)));
        }
    }

    /**
     * Initiates asynchoronous message send.
     */
    void sendMessage(
        Message msg,
        std::function<void(SystemError::ErrorCode)> handler)
    {
        if (m_sendCompletionHandler)
        {
            // TODO: #ak Remove m_sendCompletionHandler after refactoring "reverse connection" logic.
            auto auxiliaryHandler = std::exchange(m_sendCompletionHandler, nullptr);
            handler =
                [baseHandler = std::move(handler),
                    auxiliaryHandler = std::move(auxiliaryHandler)](
                        SystemError::ErrorCode code)
                {
                    baseHandler(code);
                    auxiliaryHandler(code);
                };
        }

        auto newTask = std::make_unique<SendTask>(
            std::move(msg),
            std::move(handler));
        addNewTaskToQueue(std::move(newTask));
    }

    void sendMessage(Message msg) //< Template cannot be resolved by default value.
    {
        sendMessage(
            std::move(msg),
            std::function< void(SystemError::ErrorCode) >());
    }

    void sendData(
        nx::Buffer data,
        std::function<void(SystemError::ErrorCode)> handler)
    {
        auto newTask = std::make_unique<SendTask>(
            std::move(data),
            std::move(handler));
        addNewTaskToQueue(std::move(newTask));
    }

    void setSendCompletionHandler(std::function<void(SystemError::ErrorCode)> handler)
    {
        m_sendCompletionHandler = std::move(handler);
    }

    std::chrono::milliseconds lifeDuration() const
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now() - m_creationTimestamp);
    }

    int messagesReceivedCount() const
    {
        return m_messagesReceivedCount;
    }

protected:
    virtual void processMessage(Message message) = 0;
    virtual void processSomeMessageBody(nx::Buffer /*messageBodyBuffer*/) {}
    virtual void processMessageEnd() {}

private:
    struct SendTask
    {
    public:
        SendTask() = default;

        SendTask(
            Message _msg,
            std::function<void(SystemError::ErrorCode)> _handler)
            :
            msg(std::move(_msg)),
            handler(std::move(_handler))
        {
        }

        SendTask(
            nx::Buffer _buf,
            std::function<void(SystemError::ErrorCode)> _handler)
            :
            buf(std::move(_buf)),
            handler(std::move(_handler))
        {
        }

        SendTask(SendTask&& /*right*/) = default;

        SendTask(const SendTask&) = delete;
        SendTask& operator=(const SendTask&) = delete;

        std::optional<Message> msg;
        std::optional<nx::Buffer> buf;
        std::function<void(SystemError::ErrorCode)> handler;
        bool asyncSendIssued = false;
    };

    Message m_message;
    Parser m_parser;
    Serializer m_serializer;
    SerializerState m_serializerState = SerializerState::done;
    nx::Buffer m_writeBuffer;
    std::function<void(SystemError::ErrorCode)> m_sendCompletionHandler;
    std::deque<SendTask> m_sendQueue;
    nx::utils::InterruptionFlag m_connectionFreedFlag;
    bool m_messageReported = false;
    QnByteArrayConstRef m_dataToParse;
    std::chrono::steady_clock::time_point m_creationTimestamp;
    int m_messagesReceivedCount = 0;

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
        m_dataToParse.pop_front(bytesProcessed);
        switch (parserState)
        {
            case ParserState::init:
            case ParserState::readingMessage:
                NX_ASSERT(m_dataToParse.isEmpty());
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
                ++m_messagesReceivedCount;

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
                // TODO: #ak Ignore all further data and close connection?
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
        if (msgBodyBuffer.isEmpty())
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
        m_message.clear();
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

        auto& task = m_sendQueue.front();
        task.asyncSendIssued = true;
        if (task.msg)
        {
            sendMessageInternal(*task.msg);
        }
        else if (task.buf)
        {
            NX_ASSERT(m_writeBuffer.isEmpty());
            m_writeBuffer = std::exchange(*task.buf, {});
            m_serializerState = SerializerState::done;
            base_type::sendBufAsync(m_writeBuffer);
        }
    }

    void serializeMessage()
    {
        size_t bytesWritten = 0;
        m_serializerState = m_serializer.serialize(&m_writeBuffer, &bytesWritten);
        if ((m_serializerState == SerializerState::needMoreBufferSpace) &&
            (bytesWritten == 0))
        {
            // TODO: #ak Increase buffer.
            NX_ASSERT(false);
        }
    }

    void reportErrorAndCloseConnection(SystemError::ErrorCode errorCode)
    {
        NX_ASSERT(!m_sendQueue.empty());

        auto handler = std::exchange(m_sendQueue.front().handler, nullptr);
        {
            nx::utils::InterruptionFlag::Watcher watcher(&m_connectionFreedFlag);
            handler(errorCode);
            if (watcher.interrupted())
                return; //< Connection has been removed by handler.
        }

        base_type::closeConnection(errorCode);
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
