/**********************************************************
* 20 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_PARSE_HANDLER_H
#define STUN_MESSAGE_PARSE_HANDLER_H

#include "stun_message.h"

namespace nx {
namespace stun {

    class MessageParseHandler
    {
    public:
        MessageParseHandler()
        :
            m_message( nullptr )
        {
        }

        void setMessage( Message* const message )
        {
            m_message = message;
        }

        void onMessageStart()
        {
            //TODO/IMPL
        }

        void onHeader( const Header& /*header*/ )
        {
            //TODO/IMPL
        }

        template<class AttrType>
            void onAttribute( int /*attrType*/, const AttrType& /*attr*/ )
        {
            //TODO/IMPL
        }

        void onMessageEnd( bool /*result*/ )
        {
            //TODO/IMPL
        }

    private:
        Message* m_message;
    };

} // namespase stun
} // namespase nx

#endif  //STUN_MESSAGE_PARSE_HANDLER_H
