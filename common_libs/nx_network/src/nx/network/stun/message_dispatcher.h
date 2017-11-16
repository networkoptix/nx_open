#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <nx/utils/singleton.h>
#include <nx/network/async_stoppable.h>

#include "abstract_server_connection.h"

namespace nx {
namespace stun {

/**
 * Dispatches STUN protocol messages to corresponding processor.
 * NOTE: This class methods are not thread-safe.
 */
class NX_NETWORK_API MessageDispatcher
{
public:
    using MessageProcessor = std::function<
        void(std::shared_ptr< AbstractServerConnection >, stun::Message)>;

    MessageDispatcher() = default;

    MessageDispatcher(const MessageDispatcher&) = delete;
    MessageDispatcher& operator=(const MessageDispatcher&) = delete;
    MessageDispatcher(MessageDispatcher&&) = default;
    MessageDispatcher& operator=(MessageDispatcher&&) = default;

    virtual ~MessageDispatcher() = default;

    /**
     * @param messageProcessor Ownership of this object is not passed.
     * NOTE: All processors MUST be registered before connection processing is started,
     *   since this class methods are not thread-safe.
     * @return true if requestProcessor has been registered,
     *   false if processor for method has already been registered.
     * NOTE: Message processing function MUST be non-blocking.
     */
    bool registerRequestProcessor(int method, MessageProcessor processor);
    /** Register processor that will receive all unhandled requests. */
    void registerDefaultRequestProcessor(MessageProcessor processor);

    /**
     * Pass message to corresponding processor.
     * @param message This object is not moved in case of failure to find processor.
     * @return true if request processing passed to corresponding processor and
     *   async processing has been started, false otherwise.
     */
    virtual bool dispatchRequest(
        std::shared_ptr< AbstractServerConnection > connection,
        stun::Message message) const;

private:
    std::unordered_map<int, MessageProcessor> m_processors;
    MessageProcessor m_defaultProcessor;
};

} // namespace stun
} // namespace nx
