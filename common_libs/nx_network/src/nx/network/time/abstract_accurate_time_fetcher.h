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
     * @param utcMillis UTC time received. Undefined if sysErrorCode is not SystemError::noError.
     */
    using CompletionHandler = nx::utils::MoveOnlyFunc<
        void(qint64 /*utcMillis*/, SystemError::ErrorCode /*sysErrorCode*/)>;

    virtual ~AbstractAccurateTimeFetcher() = default;

    /**
     * Initiates asynchronous time request operation.
     * NOTE: Implementation is NOT REQUIRED to support performing multiple simultaneous operations.
     * @return true if request issued successfully, otherwise false.
     */
    virtual void getTimeAsync(CompletionHandler handlerFunc) = 0;
};
