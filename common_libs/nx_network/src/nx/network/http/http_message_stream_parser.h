#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

#include "http_stream_reader.h"
#include "http_types.h"

namespace nx {
namespace network {
namespace http {

/**
 * Pushes parsed message to the next filter.
 */
class NX_NETWORK_API HttpMessageStreamParser:
    public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    virtual bool processData( const QnByteArrayConstRef& data ) override;
    virtual size_t flush() override;

    /**
     * Message is available only within nx::utils::bstream::AbstractByteStreamFilter::processData
     *   call of the next filter.
     * @return previous http message.
     */
    nx::network::http::Message currentMessage() const;

private:
    nx::network::http::HttpStreamReader m_httpStreamReader;
};

} // namespace nx
} // namespace network
} // namespace http
