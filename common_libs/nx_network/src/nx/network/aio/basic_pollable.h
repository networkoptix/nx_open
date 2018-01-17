#pragma once

#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/std/future.h>

#include "abstract_pollable.h"
#include "pollable.h"

namespace nx {
namespace network {
namespace aio {

class AIOService;

/**
 * This class implements AbstractPollable and simplifies async operation cancellation
 * by introducing BasicPollable::stopWhileInAioThread method.
 *
 * TODO #ak conflicts with Pollable. Introduce proper names for Pollable, AbstractPollable, BasicPollable.
 * Successor to this class MUST support safe object deletion while in object's aio thread.
 * QnStoppableAsync::pleaseStop and BasicPollable::stopWhileInAioThread are not called in this case.
 * So, it is recommended that:
 * - BasicPollable::stopWhileInAioThread implementation can be safely called multiple times
 * - BasicPollable::stopWhileInAioThread call is added to the destructor
 */
class NX_NETWORK_API BasicPollable:
    public AbstractPollable
{
public:
    BasicPollable(aio::AbstractAioThread* aioThread = nullptr);
    BasicPollable(
        aio::AIOService* aioService,
        aio::AbstractAioThread* aioThread);
    virtual ~BasicPollable() override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;
    virtual void pleaseStopSync(bool checkForLocks = true) override;

    virtual aio::AbstractAioThread* getAioThread() const override;
    /**
     * Generally, binding to aio thread can be done just after
     *     object creation before any usage.
     * Some implementation may allow more (e.g., binding if no async
     *     operations are scheduled at the moment)
     * NOTE: Re-binding is allowed
     */
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    bool isInSelfAioThread() const;

    void cancelPostedCalls(nx::utils::MoveOnlyFunc<void()> completionHandler);
    /**
     * NOTE: Non blocking if called within object's aio thread.
     */
    void cancelPostedCallsSync();

    template<typename Func>
    void executeInAioThreadSync(Func func)
    {
        if (isInSelfAioThread())
        {
            func();
        }
        else
        {
            nx::utils::promise<void> done;
            post(
                [&done, &func]()
                {
                    func();
                    done.set_value();
                });
            done.get_future().wait();
        }
    }

protected:
    /** Cancel your asynchronous operations here. */
    virtual void stopWhileInAioThread();

    Pollable& pollable();

private:
    mutable Pollable m_pollable;
    AIOService* m_aioService;
    nx::utils::ObjectDestructionFlag m_destructionFlag;
};

} // namespace aio
} // namespace network
} // namespace nx
