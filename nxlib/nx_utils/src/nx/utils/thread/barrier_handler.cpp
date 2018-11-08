#include "barrier_handler.h"

namespace nx {
namespace utils {

BarrierHandler::BarrierHandler(nx::utils::MoveOnlyFunc<void()> handler)
{
    //auto handlerPtr = std::make_shared<decltype(handler)>(std::move(handler));
    std::shared_ptr<nx::utils::MoveOnlyFunc<void()>> handlerPtr(
        new nx::utils::MoveOnlyFunc<void()>(std::move(handler)));
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

BarrierWaiter::BarrierWaiter():
    BarrierHandler([this](){ this->m_promise.set_value(); })
{
}

BarrierWaiter::~BarrierWaiter()
{
    m_handlerHolder.reset();
    m_promise.get_future().wait();
}

} // namespace utils
} // namespace nx
