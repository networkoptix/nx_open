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
 * class SampleClassWithAsyncOperations:
 *     public nx::network::aio::BasicPollable
 * {
 *     using base_type = nx::network::aio::BasicPollable;
 *
 * public:
 *     SampleClassWithAsyncOperations(std::unique_ptr<HttpConnection> httpConnection):
 *         m_httpConnection(std::move(httpConnection))
 *     {
 *         // Making sure this, m_httpConnection and m_timer are bound to the same AIO thread.
 *         bindToAioThread(m_httpConnection->getAioThread());
 *     }
 *
 *     virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
 *     {
 *         base_type::bindToAioThread(aioThread);
 *
 *         if (m_httpConnection)
 *             m_httpConnection->bindToAioThread(aioThread);
 *         m_timer.bindToAioThread(aioThread);
 *     }
 *
 *     void doSomethingAsync(CompletionHandler handler)
 *     {
 *         m_httpConnection->any_async_call(... handler);
 *     }
 *
 * protected:
 *     virtual void stopWhileInAioThread() override
 *     {
 *         base_type::stopWhileInAioThread();
 *
 *         m_httpConnection.reset();
 *         m_timer.pleaseStopSync();
 *     }
 *
 * private:
 *     // HttpConnection and aio::Timer inherit aio::BasicPollable.
 *
 *     std::unique_ptr<HttpConnection> m_httpConnection;
 *     aio::Timer m_timer;
 * };
 * </code></pre>
 *
 * Successor to this class MUST support safe object deletion while in object's aio thread
 * (usually, it is acheived automatically).
 * QnStoppableAsync::pleaseStop and BasicPollable::stopWhileInAioThread are not called in this case.
 *
 * TODO #ak Name conflicts with Pollable. Consider renaming Pollable, AbstractPollable, BasicPollable.
 */
class NX_NETWORK_API BasicPollable:
    public AbstractPollable
{
public:
    /**
     * @param aioThread If nullptr, then object is bound to the current AIO thread
     * (if called in AIO thread) or to a random AIO thread (if current thread is not AIO).
     */
    BasicPollable(aio::AbstractAioThread* aioThread = nullptr);
    /**
     * @param aioThread See description in the previous constructor.
     */
    BasicPollable(
        aio::AIOService* aioService,
        aio::AbstractAioThread* aioThread);
    virtual ~BasicPollable() override;

    /**
     * @param completionHandler Called in object's AIO thread (returned by getAioThread()) after
     * completion or cancellation of all scheduled asynchronous operations.
     * Object can be safely deleted after completion of this call.
     * WARNING: All usage of this object from non-AIO thread should be halted before invoking
     * this method. Otherwise, undefined behavior will happen. All usage in AIO thread is safe
     * (it will be cancelled or waited for completion).
     * NOTE: In most cases, you don't need to override this.
     * Override BasicPollable::stopWhileInAioThread instead.
     */
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    /**
     * If called within object's AIO thread (returned by getAioThread()) then cancells all
     * scheduled operations without blocking and returns immediately.
     * Otherwise, invokes BasicPollable::pleaseStop and waits for completion.
     * NOTE: In most cases, you don't need to override this.
     * Override BasicPollable::stopWhileInAioThread instead.
     */
    virtual void pleaseStopSync() override;

    virtual aio::AbstractAioThread* getAioThread() const override;
    /**
     * Generally, binding to aio thread can be done just after object creation before scheduling
     * any asynchronous operations.
     * Some implementation may allow more. E.g., binding if all async operations have completed.
     * NOTE: Calling this method while there are scheduled asynchronous operations is
     * undefined behavior.
     * NOTE: Re-binding is allowed.
     */
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    /**
     * Posts func to be called in object's AIO thread ASAP.
     * Returns immediately (without waiting for func to be executed).
     */
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;

    /**
     * If called within AIO thread (same as returned by getAioThread()) then func is executed
     * right in this call and dispatch returns after that.
     * Otherwse, invokes BasicPollable::post.
     */
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    /**
     * @return true if called within same thread as reported by getAioThread().
     */
    bool isInSelfAioThread() const;

    /**
     * @param completionHandler Will be called in object's AIO thread (returned by getAioThread())
     * after completion or cancellation of every call posted with BasicPollable::post.
     */
    void cancelPostedCalls(nx::utils::MoveOnlyFunc<void()> completionHandler);

    /**
     * When called from object's AIO thread, then cancells all posted calls immediately and returns.
     * Otherwise, invokes BasicPollable::cancelPostedCalls and waits for its completion.
     */
    void cancelPostedCallsSync();

    /**
     * If called within object's AIO thread then invokes func immediately and returns.
     * Otherwise, posts func using BasicPollable::post and waits for completion.
     */
    template<typename Func>
    auto executeInAioThreadSync(Func func) -> decltype(func())
    {
        using Result = decltype(func());

        if (isInSelfAioThread())
        {
            return func();
        }
        else
        {
            nx::utils::promise<Result> done;
            post(
                [this, &func, &done]()
                {
                    // TODO: #ak This function call is a work-around for msvc2017 bug
                    // that causes it to compile else statement in if constexpr(...) {} else {}
                    // when inside a lambda inside a template function.
                    this->executeAndSetResultToPromise(func, done);
                });
            return done.get_future().get();
        }
    }

    /**
     * @return Object, that can be used directly with AioThread::startMonitoring.
     * NOTE: If you think you need it, consider it one more time.
     * If you still think you need it, go and talk to someone about it.
     */
    Pollable& pollable();
    const Pollable& pollable() const;

protected:
    /**
     * Reimplement and cancel your asynchronous operations here.
     * NOTE: See class description for an example.
     * NOTE: Always called in object's AIO thread.
     */
    virtual void stopWhileInAioThread();

private:
    mutable Pollable m_pollable;
    AIOService* m_aioService = nullptr;
    nx::utils::ObjectDestructionFlag m_destructionFlag;

    template<typename Func, typename Promise>
    void executeAndSetResultToPromise(Func& func, Promise& promise)
    {
        using Result = decltype(func());

        if constexpr (std::is_void<Result>::value)
        {
            func();
            promise.set_value();
        }
        else
        {
            promise.set_value(func());
        }
    }
};

} // namespace aio
} // namespace network
} // namespace nx
