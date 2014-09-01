/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#ifndef STUN_MESSAGE_SERIALIZER_H
#define STUN_MESSAGE_SERIALIZER_H

#include <stdint.h>

#include <utils/network/buffer.h>

#include "stun_message.h"
#include "../connection_server/base_protocol_message_types.h"


namespace nx_stun
{
    //!Serializes message to buffer
    class MessageSerializer
    {
    public:
        //!Set message to serialize
        void setMessage( const Message& message );

        nx_api::SerializerState::Type serialize( nx::Buffer* const buffer, size_t* const bytesWritten );

        /*!
            Doesn't use \a *buf beyond its capacity
            \param buf 
            \param header
            \return true if successfully serialized \a header. Otherwise, false (e.g., not enough space in \a *buf)
            \note In case of error \a *buf value is the same on return
        */
        bool addHeader( nx::Buffer* const buf, const Header& header );
        //!Serializes \a attr to \a *buf
        /*!
            Doesn't use \a *buf beyond its capacity
            \param buf
            \param attr
            \return true if successfully serialized \a header. Otherwise, false (e.g., not enough space in \a *buf)
            \note In case of error \a *buf value is the same on return
        */
        template<class AttributeType>
            bool addAttribute( nx::Buffer* const buf, const AttributeType& attr );
    };
}

#endif  //STUN_MESSAGE_SERIALIZER_H
