/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#include "http_parser.h"


namespace nx_http
{
    MessageParser::MessageParser()
    :
        m_msg( nullptr )
    {
    }

    void MessageParser::setMessage( Message* const msg )
    {
        m_msg = msg;
    }

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
    ParserState::Type MessageParser::parse( const nx::Buffer& buf, size_t* bytesProcessed )
    {
        if( !m_httpStreamReader.parseBytes( buf, nx_http::BufferNpos, bytesProcessed ) )
            return ParserState::failed;

        switch( m_httpStreamReader.state() ) 
        {
            case HttpStreamReader::readingMessageBody:
            case HttpStreamReader::messageDone:
                *m_msg = m_httpStreamReader.message();
                return ParserState::done;

            case HttpStreamReader::parseError:
                return ParserState::failed;

            default:
                return ParserState::inProgress;
        }
    }

    //!Resets parse state and prepares for parsing different data
    void MessageParser::reset()
    {
        m_httpStreamReader.resetState();
    }
}
