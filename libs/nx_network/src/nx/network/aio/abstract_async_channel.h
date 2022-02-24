// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <memory>

#include <nx/utils/buffer.h>
#include <nx/utils/system_error.h>
#include <nx/utils/thread/cf/cfuture.h>

#include "basic_pollable.h"
#include "event_type.h"

namespace nx::network::aio {

/**
 * Interface for any entity that support asynchronous read/write operations.
 */
class NX_NETWORK_API AbstractAsyncChannel:
    public BasicPollable
{
public:
    virtual ~AbstractAsyncChannel() override = default;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) = 0;

    /**
     * Future-based wrapper for readSomeAsync.
     *
     * @returns Count of bytes transferred (i.e. second parameter of readSomeAsync handler).
     * @throws std::system_error with error code (i.e. first parameter of readSomeAsync) and
     * std::system_category() when operation fails.
     * @throws std::system_error with std::errc::operation_canceled error code when operation is
     * canceled.
     */
    virtual cf::future<std::size_t> readSome(nx::Buffer* buffer);

    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) = 0;

    /**
     * Future-based wrapper for sendAsync.
     *
     * @returns Count of bytes transferred (i.e. second parameter of sendAsync handler).
     * @throws std::system_error with error code (i.e. first parameter of sendAsync) and
     * std::system_category() when operation fails.
     * @throws std::system_error with std::errc::operation_canceled error code when operation is
     * canceled.
     */
    virtual cf::future<std::size_t> send(const nx::Buffer* buffer);

    /**
     * Cancel async socket operation. cancellationDoneHandler is invoked when cancelled.
     * @param eventType event to cancel.
     */
    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) final;

    /**
     * Future-based wrapper for cancelIOAsync.
     */
    cf::future<cf::unit> cancelIO(nx::network::aio::EventType eventType);

    /**
     * Does not block if called within object's AIO thread.
     * If called from any other thread then returns after asynchronous handler completion.
     */
    virtual void cancelIOSync(nx::network::aio::EventType eventType) final;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) = 0;

    void cancelRead() { cancelIOSync(aio::etRead); };
    void cancelWrite() { cancelIOSync(aio::etWrite); };
};

using AsyncChannelPtr = std::unique_ptr<nx::network::aio::AbstractAsyncChannel>;

} // namespace nx::network::aio
