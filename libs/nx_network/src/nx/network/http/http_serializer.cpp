// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_serializer.h"

namespace nx::network::http {

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

} // namespace nx::network::http
