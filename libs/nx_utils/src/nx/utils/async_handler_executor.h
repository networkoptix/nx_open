// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>
#include <utility>
#include <memory>

#include <nx/utils/move_only_func.h>

class QObject;
class QThread;

namespace nx::utils {

/**
 * When implementing an async operation that reports completion via a handler callback, use this
 * class to enable ergonomic invocation of the handler in a given QThread or a given QObject's
 * associated QThread.
 *
 * Define the async operation:
 * <pre><code>
 * template <typename... Args, typename Handler>
 * void doSomethingAsync(Args..., Handler&& handler, AsyncHandlerExecutor handlerExecutor = {})
 * {
 *     // Bind handler to handlerExecutor. Calling boundHandler will submit handler for execution
 *     // in appropriate thread.
 *     auto boundHandler = handlerExecutor.bind(std::forward<Handler>(handler));
 *
 *     // Forward boundHandler through the callback chains as required.
 *     // It has the came copyability as handler and can be called with the same arguments.
 *
 *     // When the operation is complete, simply invoke it (from any thread).
 *     boundHandler(results...);
 * }
 * </code></pre>
 *
 * And then use it as follows:
 * <pre><code>
 * doSomethingAsync(args...,
 *     [this](auto... results)
 *     {
 *         // Called from the thread associated with the QObject pointed to by ptrToSomeQObject.
 *         // Will not be called if the object is destroyed before the operation completes.
 *     },
 *     ptrToSomeQObject);
 * // or
 * doSomethingAsync(args...,
 *     [this](auto... results)
 *     {
 *         // Called from the thread pointed to by ptrToSomeQThread.
 *         // Will not be called if the thread is destroyed before the operation completes.
 *     },
 *     ptrToSomeQThread);
 * // or
 * doSomethingAsync(args...,
 *     [this](auto... results)
 *     {
 *         // Called by the thread that calls boundHandler, directly from within boundHandler.
 *     });
 * </code></pre>
 */
class NX_UTILS_API AsyncHandlerExecutor
{
public:
    class Impl;

    /**
     * Handlers submitted to an instance constructed with this constructor or its copies
     * will be called directly from within submit by the thread that calls submit.
     */
    AsyncHandlerExecutor();

    /**
     * Handlers submitted to an instance constructed with this constructor or its copies
     * will be called from the thread associated with object. Destroying the object is allowed,
     * submitted handlers will not be called in that case.
     */
    AsyncHandlerExecutor(QObject* object);

    /**
     * Handlers submitted to an instance constructed with this constructor or its copies
     * will be called from the specified thread. Destroying the thread is allowed,
     * submitted handlers will not be called in that case.
     */
    AsyncHandlerExecutor(QThread* thread);

    /**
     * Submit handler for execution without arguments in appropriate thread.
     *
     * @param Handler Must be movable and callable with no arguments.
     */
    template <typename Handler>
    void submit(Handler&& handler) const
    {
        submitImpl(std::forward<Handler>(handler));
    }

    /**
     * Submit handler for execution with given arguments in appropriate thread.
     *
     * @param handler Must be movable and callable with rvalues of the decayed and unwrapped
     * arguments.
     * @param args Passed to handler after decaying and unwrapping std::reference_wrappers.
     */
    template <typename Handler, typename... Args>
    void submit(Handler&& handler, Args&&... args) const
    {
        submitImpl(
            [handler = std::forward<Handler>(handler),
                args = std::make_tuple(std::forward<Args>(args)...)]() mutable
            {
                std::apply(handler, std::move(args));
            });
    }

    /**
     * Bind handler to be invoked in appropriate thread.
     *
     * @param handler Handler to wrap. Must be movable.
     * @return Bound handler of unspecified type. It is copyable if and only if handler is
     * copyable. If called with some arguments, submits handler for execution in appropriate
     * thread with those arguments as if by submit.
     */
    template <typename Handler>
    auto bind(Handler&& handler) const
    {
        return
            [*this, handler = std::forward<Handler>(handler)](auto&&... args) mutable
            {
                submit(std::move(handler), std::forward<decltype(args)>(args)...);
            };
    }

private:
    void submitImpl(MoveOnlyFunc<void()>&& handler) const;

private:
    std::shared_ptr<Impl> m_impl;
};

} // namespace nx::utils
