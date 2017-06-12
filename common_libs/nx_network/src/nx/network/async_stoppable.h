#pragma once

#include <nx/utils/move_only_func.h>

/**
 * Abstract interface to interrupt asynchronous operation with completion notification.
 */
class NX_NETWORK_API QnStoppableAsync
{
public:
    virtual ~QnStoppableAsync() = default;

    /**
     * Ask object to interrupt all asynchoronous operations.
     * Caller MUST ensure that no asynchronous operations are started after this call
     * @param completionHandler Executed when asynchronous operation is interrupted. 
     *   For example, in case with async socket operations,
     *   completionHandler is triggered when socket completion handler returned or will never be called.
     *   Allowed to be \a null.
     * NOTE: If operation is already stopped it is allowed for completionHandler 
     *   to be executed directly in QnStoppableAsync::pleaseStop.
     */
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) = 0;

    /**
     * Stops object's asynchronous operations and waits for completion.
     * Default implementation calls QnStoppableAsync::pleaseStop and waits for completion.
     */
    virtual void pleaseStopSync(bool checkForLocks = true);
};
