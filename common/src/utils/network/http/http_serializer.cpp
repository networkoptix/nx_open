/**********************************************************
* 23 dec 2013
* a.kolesnikov
***********************************************************/

#include "http_serializer.h"


namespace nx_http
{
    MessageSerializer::MessageSerializer()
    {
    }

    //!Set message to serialize
    void MessageSerializer::setMessage( Message&& message )
    {
        m_message = std::move(message);
    }
        
    SerializerState::Type MessageSerializer::serialize( nx::Buffer* const buffer, size_t* const bytesWritten )
    {
        //TODO #ak introduce cool implementation (small refactor to HttpMessage::serialize is required)

        const auto bufSizeBak = buffer->size();
        m_message.serialize( buffer );
        *bytesWritten = buffer->size() - bufSizeBak;
        return SerializerState::done;
    }
}
