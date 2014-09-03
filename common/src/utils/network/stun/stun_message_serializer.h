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
        void setMessage( Message&& message );

        nx_api::SerializerState::Type serialize( nx::Buffer* const buffer, size_t* const bytesWritten );
    };
}

#endif  //STUN_MESSAGE_SERIALIZER_H
