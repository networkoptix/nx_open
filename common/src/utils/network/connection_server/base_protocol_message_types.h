/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef BASE_STREAM_MESSAGE_TYPES_H
#define BASE_STREAM_MESSAGE_TYPES_H

#include <utils/network/buffer.h>
#include <array>
#include <utility>
#include <vector>
#include <memory>

namespace nx_api
{
    namespace ParserState
    {
        typedef int Type;

        static const int init = 1;
        static const int inProgress = 2;
        static const int done = 3;
        static const int failed = 4;
    };

    namespace SerializerState
    {
        typedef int Type;

        static const int needMoreBufferSpace = 1;
        static const int done = 2;
    };
    
    class Message {
    public:
        void clear();
    };

    //!Demonstrates API of message parser
    class MessageParser
    {
    public:
        MessageParser();
        //!Set message object to parse to
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
        ParserState::Type parse( const nx::Buffer& /*buf*/, size_t* /*bytesProcessed*/ );

        //!Resets parse state and prepares for parsing different data
        void reset();
    };

    //!Demonstrates API of message serializer
    class MessageSerializer
    {
    public:
        //!Set message to serialize
        void setMessage( Message&& message );

        SerializerState::Type serialize( nx::Buffer* const buffer, size_t* bytesWritten );
    };
}

#endif  //BASE_STREAM_MESSAGE_TYPES_H
