#pragma once

#include <functional>

#include <nx/utils/system_error.h>

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
        IoCompletionHandler handler) = 0;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) = 0;

    /**
     * Does not block if called within object's aio thread.
     * If called from any other thread then returns after asynchronous handler completion.
     */
    virtual void cancelIOSync(nx::network::aio::EventType eventType) = 0;
};

} // namespace aio
} // namespace network
} // namespace nx
