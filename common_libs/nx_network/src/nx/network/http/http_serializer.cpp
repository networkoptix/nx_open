#include "http_serializer.h"

namespace nx_http {

void MessageSerializer::setMessage(const Message* message)
{
    m_message = message;
}

nx::network::server::SerializerState MessageSerializer::serialize(
    nx::Buffer* const buffer,
    size_t* bytesWritten)
{
    const auto bufSizeBak = buffer->size();
    m_message->serialize(buffer);
    *bytesWritten = buffer->size() - bufSizeBak;
    return nx::network::server::SerializerState::done;
}

} // namespace nx_http
