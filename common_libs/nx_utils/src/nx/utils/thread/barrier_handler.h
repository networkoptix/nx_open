#ifndef NX_UTILS_BARRIER_HANDLER_H
#define NX_UTILS_BARRIER_HANDLER_H

#include <functional>
#include <memory>

#include "mutex.h"
#include "../move_only_func.h"


namespace nx {

/** Forks handler into several handlers and call the last one when
 *  the last fork handler is called */
class NX_UTILS_API BarrierHandler
{
public:
    BarrierHandler(nx::utils::MoveOnlyFunc<void()> handler );
    std::function<void()> fork();

private:
    std::shared_ptr<BarrierHandler> m_handlerHolder;
};

} // namespace nx

#endif // NX_UTILS_BARRIER_HANDLER_H
