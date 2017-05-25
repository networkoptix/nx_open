#pragma once 

#include "http_stream_reader.h"
#include "http_types.h"
#include "../buffer.h"
#include "../connection_server/base_protocol_message_types.h"

namespace nx_http {

/**
 * This class is just a wrapper for use with nx::network::server::BaseStreamProtocolConnection class. 
 */
class NX_NETWORK_API MessageParser
{
public:
    MessageParser();

    void setMessage(Message* const msg);
    /**
     * Methods returns if:
     *   - end of message found
     *   - source data depleted
     *
     * @param bytesProcessed Number of bytes from buf which were read and parsed is stored here.
     * Returns current parse state.
     * NOTE: *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte).
     * NOTE: Reads whole message even if parse error occured.
     */
    nx::network::server::ParserState parse(const nx::Buffer& buf, size_t* bytesProcessed);
    nx::network::server::ParserState processEof();

    /** Resets parse state and prepares for parsing different data. */
    void reset();

private:
    HttpStreamReader m_httpStreamReader;
    Message* m_msg;
};

} // namespace nx_http
