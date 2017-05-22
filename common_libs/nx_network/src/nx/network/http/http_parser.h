#pragma once 

#include "httpstreamreader.h"
#include "http_types.h"
#include "../buffer.h"
#include "../connection_server/base_protocol_message_types.h"

namespace nx_http {

/**
 * This class is just a wrapper for use with \a nx::network::server::BaseStreamProtocolConnection class. 
 */
class NX_NETWORK_API MessageParser
{
public:
    MessageParser();

    void setMessage( Message* const msg );
    //!Returns current parse state
    /*!
        Methods returns if:\n
            - end of message found
            - source data depleted

        \param buf
        \param bytesProcessed Number of bytes from \a buf which were read and parsed is stored here
        \note \a *buf MAY NOT contain whole message, but any part of it (it can be as little as 1 byte)
        \note Reads whole message even if parse error occured
    */
    nx::network::server::ParserState parse( const nx::Buffer& buf, size_t* bytesProcessed );
    nx::network::server::ParserState processEof();

    //!Resets parse state and prepares for parsing different data
    void reset();

private:
    HttpStreamReader m_httpStreamReader;
    Message* m_msg;
};

} // namespace nx_http
