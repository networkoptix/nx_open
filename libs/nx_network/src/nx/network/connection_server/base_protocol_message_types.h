#pragma once

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include <nx/network/buffer.h>

namespace nx {
namespace network {
namespace server {

enum class ParserState
{
    init = 1,
    readingMessage,
    readingBody,
    done,
    failed,
};

enum class SerializerState
{
    needMoreBufferSpace = 1,
    done = 2,
};

/**
 * Demonstrates API of message parser.
 */
template<typename Message>
class AbstractMessageParser
{
public:
    virtual ~AbstractMessageParser() = default;

    /** Set message object to parse to. */
    virtual void setMessage(Message* const message) = 0;

    /**
     * Returns current parse state.
     * Methods returns if:
     *   - end of message found
     *   - source data depleted
     * @param bytesProcessed Number of bytes from buf which were read and parsed is stored here.
     * NOTE: *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte).
     * NOTE: Reads whole message even if parse error occured.
     */
    virtual ParserState parse(const nx::Buffer& /*buf*/, size_t* /*bytesProcessed*/) = 0;

    /**
     * If parser supports streaming message body
     *   (AbstractMessageParser::parse implementation can return ParserState::readingBody),
     *   then it should be returned here.
     * If parser does not support it (never reports ParserState::readingBody) state,
     *   then it should leave dummy implementation.
     */
    virtual nx::Buffer fetchMessageBody() { return nx::Buffer(); };

    /** Resets parse state and prepares for parsing different data. */
    virtual void reset() = 0;
};

/**
 * Demonstrates API of message serializer.
 */
template<typename Message>
class AbstractMessageSerializer
{
public:
    virtual ~AbstractMessageSerializer() = default;

    /** Set message to serialize. */
    virtual void setMessage(const Message* message) = 0;

    virtual SerializerState serialize(
        nx::Buffer* const buffer,
        size_t* bytesWritten) = 0;
};

} // namespace server
} // namespace network
} // namespace nx
