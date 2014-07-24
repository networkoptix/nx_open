/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "httpstreamreader.h"
#include "httptypes.h"
#include "../buffer.h"


namespace nx_http
{
    namespace ParserState
    {
        typedef int Type;

        static const int init = 1;
        static const int inProgress = 2;
        static const int done = 3;
        static const int failed = 4;
    };

    /*!
        This class is just a wrapper for use with \a nx_api::BaseStreamProtocolConnection class. 
        \todo get rid of this!
    */
    class MessageParser
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
        ParserState::Type parse( const nx::Buffer& buf, size_t* bytesProcessed );

        //!Resets parse state and prepares for parsing different data
        void reset();

    private:
        HttpStreamReader m_httpStreamReader;
        Message* m_msg;
    };
}

#endif  //HTTP_PARSER_H
