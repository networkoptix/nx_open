#pragma once

#include <nx/utils/move_only_func.h>

namespace nx { namespace network { namespace aio { class AIOService; } } }

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
     *   Allowed to be null.
     * NOTE: If operation is already stopped it is allowed for completionHandler
     *   to be executed directly in QnStoppableAsync::pleaseStop.
     */
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) = 0;

    /**
     * Stops object's asynchronous operations and waits for completion.
     * Default implementation Cannot be called in aio thread.
     * Also, it is not recommended to call default implemntation with mutex locked.
     * It is a potential deadlock.
     */
    virtual void pleaseStopSync(bool checkForLocks = true);

protected:
    /**
     * Calls QnStoppableAsync::pleaseStop and waits for completion.
     * Also, contains some asserts.
     */
    void pleaseStopSync(
        const nx::network::aio::AIOService* aioService,
        bool checkForLocks);
};
