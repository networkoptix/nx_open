#pragma once

#include <QtGlobal>

#include <nx/utils/move_only_func.h>

#include "../aio/basic_pollable.h"

/**
 * Abstract interface for class performing fetching accurate current time from some public server.
 */
class NX_NETWORK_API AbstractAccurateTimeFetcher:
    public nx::network::aio::BasicPollable
{
public:
    /**
     * Functor arguments:
     * - qint64 on success UTC millis from epoch. On failure -1.
     * - SystemError::ErrorCode last system error code. SystemError::noError in case of success.
     */
    using CompletionHandler = nx::utils::MoveOnlyFunc<void(qint64, SystemError::ErrorCode)>;

    virtual ~AbstractAccurateTimeFetcher() = default;

    /**
     * Initiates asynchronous time request operation.
     * @note Implementation is NOT REQUIRED to support performing multiple simultaneous operations.
     * @return true if request issued successfully, otherwise false.
     */
    virtual void getTimeAsync(CompletionHandler handlerFunc) = 0;
};
