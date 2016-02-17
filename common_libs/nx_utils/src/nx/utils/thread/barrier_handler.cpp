#include "barrier_handler.h"

namespace nx {

BarrierHandler::BarrierHandler(nx::utils::MoveOnlyFunc<void()> handler)
{
    auto handlerPtr = std::make_shared<decltype(handler)>(std::move(handler));
    m_handlerHolder = std::shared_ptr<BarrierHandler>(
        this,
        [handlerPtr]( //not passing handler directly since std::shared_ptr's deleter MUST be CopyConstructible
            BarrierHandler*)
        {
            (*handlerPtr)();
        });
}

std::function<void()> BarrierHandler::fork()
{
    // TODO: move lambda
    auto holder = std::make_shared<
            std::shared_ptr<BarrierHandler>>(m_handlerHolder);
    return [holder]() { holder->reset(); };
}

} // namespace nx
