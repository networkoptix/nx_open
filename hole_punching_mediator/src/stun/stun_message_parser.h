/**********************************************************
* 18 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_PARSER_H
#define STUN_MESSAGE_PARSER_H

#include <stdint.h>

#include <utils/network/abstract_socket.h>

#include "stun_message.h"


namespace nx_stun
{
    enum class ParserState
    {
        init,
        inProgress,
        done,
        //!goes to this state if data is not STUN
        failed
    };

    //!STUN message parser (event-based)
    /*!
        \a Handler MUST methods with following signature:
        \code {.cpp}
            void onMessageStart();
            void onHeader( const stun::Header& );
            template<class AttrType>
                void onAttribute( int attrType, const AttrType& attr );
            void onMessageEnd( bool result );    //true on success
        \endcode

        Text attributes (e.g. USERNAME, REALM, SOFTWARE, etc..) have type \a std::string
    */
    template<class Handler>
    class MessageParser
    {
    public:
        MessageParser( Handler* const hander )
        :
            m_hander( hander )
        {
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
        ParserState parse( const nx::Buffer& /*buf*/, size_t* /*bytesProcessed*/ )
        {
            //TODO/IMPL
            return state();
        }

        //!Returns current parse state
        ParserState state() const
        {
            //TODO/IMPL
            return ParserState::init;
        }

        //!Resets parse state and prepares for parsing different data
        void reset()
        {
            //TODO/IMPL
        }

    private:
        Handler* const m_hander;
    };
}

#endif  //STUN_MESSAGE_PARSER_H
