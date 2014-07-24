/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#include "http_serializer.h"


namespace nx_http
{
    MessageSerializer::MessageSerializer()
    :
        m_message( nullptr )
    {
    }

    //!Set message to serialize
    void MessageSerializer::setMessage( const Message& message )
    {
        m_message = &message;
    }
        
    SerializerState::Type MessageSerializer::serialize( nx::Buffer* const buffer, size_t* const bytesWritten )
    {
        //TODO/IMPL: #ak introduce cool implementation (small refactor to HttpMessage::serialize is required)

        nx_http::BufferType tmpBuf;
        m_message->serialize( &tmpBuf );
        if( tmpBuf.size() < buffer->capacity() - buffer->size() )
            return SerializerState::needMoreBufferSpace;

        buffer->append( tmpBuf );
        *bytesWritten = tmpBuf.size();
        return SerializerState::done;
    }
}
