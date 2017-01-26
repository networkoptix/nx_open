#pragma once

#include <QtGlobal>

#include <nx/utils/move_only_func.h>

#include "../aio/basic_pollable.h"

/**
 * Abstract interface for class performing fetching accurate current time from some public server.
 */
class NX_NETWORK_API AbstractAccurateTimeFetcher
:
    public nx::network::aio::BasicPollable
{
public:
    /**
     * Functor arguments:
     * - \a qint64 on success UTC millis from epoch. On failure -1.
     * - \a SystemError::ErrorCode last system error code. \a SystemError::noError in case of success.
     */
    typedef nx::utils::MoveOnlyFunc<void(qint64, SystemError::ErrorCode)>
        CompletionHandler;

    virtual ~AbstractAccurateTimeFetcher() = default;

    /**
     * Initiates asynchronous time request operation.
     * @note Implementation is NOT REQUIRED to support performing multiple simultaneous operations.
     * @return \a true if request issued successfully, otherwise \a false.
     */
    virtual void getTimeAsync(CompletionHandler handlerFunc) = 0;
};
