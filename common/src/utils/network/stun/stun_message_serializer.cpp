/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_message_serializer.h"


namespace nx_stun
{
    void MessageSerializer::setMessage( Message&& message )
    {
        //TODO/IMPL
    }

    nx_api::SerializerState::Type MessageSerializer::serialize( nx::Buffer* const buffer, size_t* const bytesWritten )
    {
        //TODO/IMPL
        return nx_api::SerializerState::done;
    }
}
