// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <nx/network/async_stoppable.h>
#include <nx/utils/singleton.h>

#include "abstract_message_handler.h"
#include "abstract_server_connection.h"

namespace nx::network::stun {

/**
 * Dispatches STUN protocol messages to corresponding processor.
 * NOTE: This class methods are not thread-safe.
 */
class NX_NETWORK_API MessageDispatcher:
    public AbstractMessageHandler
{
public:
    using MessageProcessor = std::function<void(MessageContext)>;

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
     * Pass message to handler registered for the message method.
     * If no handler was matched, a notFound reply is sent.
     */
    virtual void serve(MessageContext ctx) override;

private:
    std::unordered_map<int, MessageProcessor> m_processors;
    MessageProcessor m_defaultProcessor;
};

} // namespace nx::network::stun
