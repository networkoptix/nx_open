/**********************************************************
* Jun 30, 2016
* akolesnikov
***********************************************************/

#pragma once

#include "abstract_pollable.h"

#include "timer.h"

namespace nx {
namespace network {
namespace aio {

/**
 * This class implements \a AbstractPollable and simplifies async operation cancellation 
 *     by introducing \a BasicPollable::stopWhileInAioThread method.
 *
 * TODO #ak conflicts with Pollable. Introduce proper names for Pollable, AbstractPollable, BasicPollable.
 * Successor to this class MUST support safe object deletion while in object's aio thread.
 * \a QnStoppableAsync::pleaseStop and \a BasicPollable::stopWhileInAioThread 
 *     are not called in this case. 
 * So, it is recommended that
 * - \a BasicPollable::stopWhileInAioThread implementation can be safely called multiple times
 * - \a BasicPollable::stopWhileInAioThread call is added to the destructor
 */
class NX_NETWORK_API BasicPollable
:
    public AbstractPollable
{
public:
    BasicPollable(aio::AbstractAioThread* aioThread = nullptr);

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    void pleaseStopSync(bool checkForLocks = true) override;

    virtual aio::AbstractAioThread* getAioThread() const override;
    /**
     * Generally, binding to aio thread can be done just after 
     *     object creation before any usage.
     * Some implementation may allow more (e.g., binding if no async 
     *     operations are scheduled at the moment)
     * @note Re-binding is allowed
     */
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

protected:
    Timer* timer();

    /** Cancel your asynchronous operations here. */
    virtual void stopWhileInAioThread() = 0;

private:
    Timer m_timer;
};

} // namespace aio
} // namespace network
} // namespace nx
