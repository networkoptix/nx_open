#pragma once

#include <nx/network/connection_server/base_protocol_message_types.h>

#include "http_types.h"
#include "../buffer.h"

namespace nx {
namespace network {
namespace http {

class NX_NETWORK_API MessageSerializer:
    public nx::network::server::AbstractMessageSerializer<Message>
{
public:
    virtual void setMessage(const Message* message) override;

    virtual nx::network::server::SerializerState serialize(
        nx::Buffer* const buffer,
        size_t* bytesWritten) override;

private:
    const Message* m_message = nullptr;
};

} // namespace nx
} // namespace network
} // namespace http
