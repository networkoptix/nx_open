#pragma once

#include <functional>
#include <memory>

#include "mutex.h"
#include "../move_only_func.h"
#include "../std/future.h"

namespace nx {
namespace utils {

/**
 * Forks handler into several handlers and call the initial handler when the last fork handler is
 * called.
 */
class NX_UTILS_API BarrierHandler
{
public:
    BarrierHandler(nx::utils::MoveOnlyFunc<void()> handler);
    std::function<void()> fork();

protected:
    std::shared_ptr<BarrierHandler> m_handlerHolder;
};

/**
 * Forks handler into several handlers and waits for all of them to be called in destructor.
 */
class NX_UTILS_API BarrierWaiter:
    public BarrierHandler
{
public:
    BarrierWaiter();
    ~BarrierWaiter();

private:
    nx::utils::promise<void> m_promise;
};

} // namespace utils
} // namespace nx
