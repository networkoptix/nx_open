#pragma once

#include <functional>

#include <utils/common/systemerror.h>

#include "basic_pollable.h"
#include "event_type.h"
#include "../buffer.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Interface for any entity that support asynchronous read/write operations.
 */
class AbstractAsyncChannel:
    public BasicPollable
{
public:
    virtual ~AbstractAsyncChannel() override = default;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) = 0;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) = 0;

    virtual void cancelIOSync(nx::network::aio::EventType eventType) = 0;
};

} // namespace aio
} // namespace network
} // namespace nx
