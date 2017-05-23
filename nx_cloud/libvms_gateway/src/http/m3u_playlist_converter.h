#pragma once

#include "message_body_converter.h"

namespace nx {
namespace cloud {
namespace gateway {

class M3uPlaylistConverter:
    public AbstractMessageBodyConverter
{
public:
    M3uPlaylistConverter(const nx::String& targetHost);

    virtual nx_http::BufferType convert(nx_http::BufferType originalBody) override;

private:
    const nx::String m_targetHost;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
