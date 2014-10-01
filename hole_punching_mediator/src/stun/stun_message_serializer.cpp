/**********************************************************
* 19 dec 2013
* a.kolesnikov
***********************************************************/

#include "stun_message_serializer.h"


namespace nx_stun
{
    void MessageSerializer::setMessage( const Message& message )
    {
        //TODO/IMPL
    }

    nx_api::SerializerState::Type MessageSerializer::serialize( nx::Buffer* const buffer, size_t* const bytesWritten )
    {
        //TODO/IMPL
        return nx_api::SerializerState::done;
    }

    /*!
        Doesn't use \a *buf beyond its capacity
        \param buf 
        \param header
        \return true if successfully serialized \a header. Otherwise, false (e.g., not enough space in \a *buf)
        \note In case of error \a *buf value is the same on return
    */
    bool MessageSerializer::addHeader( nx::Buffer* const buf, const Header& header )
    {
        //TODO/IMPL
        return false;
    }
}
