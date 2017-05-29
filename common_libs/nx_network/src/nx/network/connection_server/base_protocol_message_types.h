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
    inProgress,
    done,
    failed,
};

namespace SerializerState
{
    using Type = int;

    static const int needMoreBufferSpace = 1;
    static const int done = 2;
}; // namespace SerializerState
    
class Message
{
public:
    void clear();
};

/**
 * Demonstrates API of message parser.
 */
class MessageParser
{
public:
    MessageParser();

    /** Set message object to parse to. */
    void setMessage(Message* const msg);
    /**
     * Returns current parse state.
     * Methods returns if:
     *   - end of message found
     *   - source data depleted
     * @param bytesProcessed Number of bytes from buf which were read and parsed is stored here.
     * NOTE: *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte).
     * NOTE: Reads whole message even if parse error occured.
     */
    ParserState parse(const nx::Buffer& /*buf*/, size_t* /*bytesProcessed*/);

    /** Resets parse state and prepares for parsing different data. */
    void reset();
};

/**
 * Demonstrates API of message serializer.
 */
class MessageSerializer
{
public:
    /** Set message to serialize. */
    void setMessage(const Message* message);

    SerializerState::Type serialize(nx::Buffer* const buffer, size_t* bytesWritten);
};

} // namespace server
} // namespace network
} // namespace nx
