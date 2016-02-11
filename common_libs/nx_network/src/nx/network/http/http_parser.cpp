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
            //TODO #ak currently, always reading full message before going futher.
            //  Have to add support for infinite request message body to async server
            //case HttpStreamReader::pullingLineEndingBeforeMessageBody:
            //case HttpStreamReader::readingMessageBody:
            case HttpStreamReader::messageDone:
                *m_msg = m_httpStreamReader.message();
                if (m_msg->type == MessageType::request)
                    m_msg->request->messageBody = m_httpStreamReader.fetchMessageBody();
                else if (m_msg->type == MessageType::response)
                    m_msg->response->messageBody = m_httpStreamReader.fetchMessageBody();
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
