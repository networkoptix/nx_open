// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

#include "http_stream_reader.h"
#include "http_types.h"

namespace nx::network::http {

/**
 * Pushes parsed message body to the next filter.
 */
class NX_NETWORK_API HttpMessageStreamParser:
    public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    virtual bool processData( const nx::ConstBufferRefType& data ) override;
    virtual size_t flush() override;

    /**
     * Message is available only within nx::utils::bstream::AbstractByteStreamFilter::processData
     *   call of the next filter.
     * @return Current HTTP message.
     */
    nx::network::http::Message currentMessage() const;

private:
    nx::network::http::HttpStreamReader m_httpStreamReader;
};

} // namespace nx::network::http
