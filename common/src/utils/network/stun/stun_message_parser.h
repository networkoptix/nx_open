/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_PARSER_H
#define STUN_MESSAGE_PARSER_H

#include <stdint.h>
#include <memory>
#include <utils/network/abstract_socket.h>

#include "../connection_server/base_protocol_message_types.h"


namespace nx_stun
{
    class Message;
    namespace detail {
        class MessageParserImpl;
        struct MessageParserImplPtr : public std::unique_ptr<MessageParserImpl> {
            MessageParserImplPtr( MessageParserImpl* );
            ~MessageParserImplPtr();
        };
    }

    class MessageParser
    {
    public:
        MessageParser();
        ~MessageParser();

        //!set message object to parse to
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
        nx_api::ParserState::Type parse( const nx::Buffer& /*buf*/, size_t* /*bytesProcessed*/ );

        //!Returns current parse state
        nx_api::ParserState::Type state() const;

        //!Resets parse state and prepares for parsing different data
        void reset();
    private:

        detail::MessageParserImplPtr impl_;
    };
}

#endif  //STUN_MESSAGE_PARSER_H
