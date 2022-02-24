// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>
#include <optional>
#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "http_types.h"

namespace nx::network::http {

/**
 * Interface for classes that provide HTTP message body.
 * If AbstractMsgBodySource::contentLength returns a value then exactly
 * specified number of bytes will be read by a sequence of AbstractMsgBodySource::readAsync calls.
 * Otherwise, data is read until empty buffer is received or error occurs.
 */
class NX_NETWORK_API AbstractMsgBodySource:
    public nx::network::aio::BasicPollable
{
public:
    using CompletionHandler =
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, nx::Buffer)>;

    virtual ~AbstractMsgBodySource() = default;

    /**
     * @return Full MIME type of content. E.g., application/octet-stream.
     */
    virtual std::string mimeType() const = 0;

    /**
     * @return length of content, provided by this data source.
     * NOTE: The implementation MUST be non-blocking and have constant complexity!
     * E.g., invoking stat() to get file size in this method must be avoided.
     */
    virtual std::optional<uint64_t> contentLength() const = 0;

    /**
     * NOTE: The descendant can invoke completionHandler directly in this call.
     * NOTE: End-of-data is signalled with (SystemError::noError, {empty buffer}).
     */
    virtual void readAsync(CompletionHandler completionHandler) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Interface for classes that provide HTTP message body and support non-blocking read.
 * E.g., class that reads from in-memory buffer should implement this class.
 */
class NX_NETWORK_API AbstractMsgBodySourceWithCache:
    public AbstractMsgBodySource
{
public:
    /**
     * @return The data that is already cached by the object.
     * In case if there is not data cached std::nullopt is returned.
     * NOTE: Can be invoked only if there is no scheduled AbstractMsgBodySource::readAsync call.
     * NOTE: The function is not thread-safe.
     */
    virtual std::optional<std::tuple<SystemError::ErrorCode, nx::Buffer>> peek() = 0;

    /**
     * Implements the same behavior as AbstractAsyncChannel::readSomeAsync.
     * NOTE: It is a wrapper over AbstractMsgBodySource::readAsync.
     */
    void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler);

    /**
     * Cancels the pending readAsync. Follows the behavior of AbstractAsyncChannel::cancelIOSync.
     */
    virtual void cancelRead() = 0;
};

} // namespace nx::network::http
