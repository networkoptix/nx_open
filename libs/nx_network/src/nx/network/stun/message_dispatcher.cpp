#include "message_dispatcher.h"

namespace nx {
namespace network {
namespace stun {

bool MessageDispatcher::registerRequestProcessor(
    int method, MessageProcessor processor)
{
    return m_processors.emplace(method, std::move(processor)).second;
}

void MessageDispatcher::registerDefaultRequestProcessor(
    MessageProcessor processor)
{
    m_defaultProcessor = std::move(processor);
}

bool MessageDispatcher::dispatchRequest(
    std::shared_ptr< AbstractServerConnection > connection,
    stun::Message message) const
{
    const auto it = m_processors.find(message.header.method);
    const MessageProcessor& processor =
        it == m_processors.end()
        ? m_defaultProcessor
        : it->second;
    if (!processor)
        return false;

    processor(std::move(connection), std::move(message));
    return true;
}

} // namespace stun
} // namespace network
} // namespace nx
