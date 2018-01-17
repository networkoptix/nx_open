#pragma once

#include <cstdint>

#include <boost/optional.hpp>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "http_types.h"

namespace nx {
namespace network {
namespace http {

/**
 * If AbstractMsgBodySource::contentLength returns existing value then exactly
 * specified number of bytes is fetched using AbstractMsgBodySource::readAsync.
 * Otherwise, data is fetched until empty buffer is received or error occurs.
 */
class NX_NETWORK_API AbstractMsgBodySource:
    public nx::network::aio::BasicPollable
{
public:
    virtual ~AbstractMsgBodySource() = default;

    // TODO: #ak Introduce convenient type for MIME type.
    /**
     * Returns full MIME type of content. E.g., application/octet-stream.
     */
    virtual StringType mimeType() const = 0;

    /**
     * Returns length of content, provided by this data source.
     * MUST be non-blocking and have constant complexity!
     */
    virtual boost::optional<uint64_t> contentLength() const = 0;

    /**
     * @param completionHandler can be invoked from within this call.
     * NOTE: End-of-data is signalled with (SystemError::noError, {empty buffer}).
     */
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)
        > completionHandler) = 0;
};

} // namespace nx
} // namespace network
} // namespace http
