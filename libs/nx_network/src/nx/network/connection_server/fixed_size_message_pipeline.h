#pragma once

#include "base_stream_protocol_connection.h"
#include "base_protocol_message_types.h"

namespace nx::network::server {

struct FixedSizeMessage
{
    nx::Buffer data;
};

class FixedSizeMessageParser;
class FixedSizeMessageSerializer;

//-------------------------------------------------------------------------------------------------

/**
 * Sends/receives message in following format:
 * - 4 bytes. Message length in network byte order not including this field.
 * - message
 */
using FixedSizeMessagePipeline = StreamProtocolConnection<
    FixedSizeMessage,
    FixedSizeMessageParser,
    FixedSizeMessageSerializer>;

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API FixedSizeMessageParser:
    public AbstractMessageParser<FixedSizeMessage>
{
public:
    FixedSizeMessageParser();

    virtual void setMessage(FixedSizeMessage* message) override;

    virtual ParserState parse(
        const nx::Buffer& buf,
        size_t* bytesProcessed) override;

    virtual nx::Buffer fetchMessageBody() override;

    virtual void reset() override;

private:
    enum class State
    {
        readingMessageSize,
        readingMessage,
    };

    State m_state = State::readingMessageSize;
    FixedSizeMessage* m_message = nullptr;
    nx::Buffer m_messageSizeBuffer;
    int m_lastMessageSize = 0;

    bool readMessageSize(QnByteArrayConstRef* readBuffer);
    bool readMessage(QnByteArrayConstRef* readBuffer);
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API FixedSizeMessageSerializer:
    public AbstractMessageSerializer<FixedSizeMessage>
{
public:
    virtual void setMessage(const FixedSizeMessage* message) override;

    virtual SerializerState serialize(
        nx::Buffer* const buffer,
        size_t* bytesWritten) override;

private:
    const FixedSizeMessage* m_message = nullptr;
};

} // namespace nx::network::server
