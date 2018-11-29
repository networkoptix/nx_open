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
 * This class should be inherited by any class that tends to provide asynchronous operation 
 * through nx::network::aio.
 *
 * It implements AbstractPollable and simplifies asynchronous operation cancellation by introducing 
 * BasicPollable::stopWhileInAioThread method.
 * It is recommended that BasicPollable::stopWhileInAioThread implementation can be safely 
 * called multiple times.
 * 
 * Sample implementation:
 * <pre><code>
 * class Sample:
 *     public nx::network::aio::BasicPollable
 * {
 *     using base_type = nx::network::aio::BasicPollable;
 * 
 * public:
 *     virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
 *     {
 *         base_type::bindToAioThread(aioThread);
 *        
 *         if (m_connection)
 *             m_connection->bindToAioThread(aioThread);
 *     }
 *
 *     void doSomethingAsync(CompletionHandler handler)
 *     {
 *         m_connection = ...; //< Creating connection.
 *         m_connection->any_async_call(... handler);
 *     }
 *
 * protected:
 *     virtual void stopWhileInAioThread() override
 *     {
 *         base_type::stopWhileInAioThread();
 *         m_connection->pleaseStopSync(); // or m_connection.reset();
 *     }
 * 
 * private:
 *     std::unique_ptr<AbstractStreamSocket> m_connection;
 * };
 * </code></pre>
 *
 * Successor to this class MUST support safe object deletion while in object's aio thread 
 * (usually, it is acheived automatically).
 * QnStoppableAsync::pleaseStop and BasicPollable::stopWhileInAioThread are not called in this case.
 *
 * TODO #ak conflicts with Pollable. Consider renaming Pollable, AbstractPollable, BasicPollable.
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
    /**
     * NOTE: Non blocking when called within object's aio thread.
     */
    virtual void pleaseStopSync() override;

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

    /**
     * NOTE: Non blocking when called within object's aio thread.
     */
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

    Pollable& pollable();
    const Pollable& pollable() const;

protected:
    /** Reimplement and cancel your asynchronous operations here. */
    virtual void stopWhileInAioThread();

private:
    mutable Pollable m_pollable;
    AIOService* m_aioService = nullptr;
    nx::utils::ObjectDestructionFlag m_destructionFlag;
};

} // namespace aio
} // namespace network
} // namespace nx
