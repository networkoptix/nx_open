// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_dispatcher.h"

#include <nx/utils/log/log.h>

namespace nx::network::stun {

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

void MessageDispatcher::serve(MessageContext ctx)
{
    const auto it = m_processors.find(ctx.message.header.method);
    const MessageProcessor& processor =
        it == m_processors.end()
        ? m_defaultProcessor
        : it->second;

    NX_VERBOSE(this, "ServerConnection %1. Dispatching request %2",
        ctx.connection.get(), ctx.message.header.method);

    if (processor)
        return processor(std::move(ctx));

    stun::Message response(stun::Header(
        stun::MessageClass::errorResponse,
        ctx.message.header.method,
        std::move(ctx.message.header.transactionId)));

    // TODO: verify with RFC
    response.newAttribute< stun::attrs::ErrorCode >(
        stun::error::notFound, "Method is not supported");

    ctx.connection->sendMessage(std::move(response), nullptr);
}

} // namespace nx::network::stun
