// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_message_pipeline.h"

#include <nx/network/aio/async_channel_bridge.h>
#include <nx/utils/log/log.h>

namespace nx::network::http {

AsyncMessagePipeline::~AsyncMessagePipeline() = default;

void AsyncMessagePipeline::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    for (auto& [id, ctx]: m_sendBodyOperations)
    {
        ctx->msgBody->bindToAioThread(aioThread);
        ctx->bridge->bindToAioThread(aioThread);
    }
}

void AsyncMessagePipeline::sendMessage(
    Message msg,
    std::unique_ptr<AbstractMsgBodySourceWithCache> msgBody,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    if (msgBody)
        msgBody->bindToAioThread(getAioThread());

    sendMessage(
        std::move(msg),
        [this, msgBody = std::move(msgBody), handler = std::move(handler)](
            SystemError::ErrorCode resultCode) mutable
        {
            if (resultCode != SystemError::noError || !msgBody)
                return handler(resultCode);

            sendBodyAsync(std::move(msgBody), std::move(handler));
        });
}

void AsyncMessagePipeline::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_sendBodyOperations.clear();
}

void AsyncMessagePipeline::sendBodyAsync(
    std::unique_ptr<AbstractMsgBodySourceWithCache> msgBody,
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    auto bridge = aio::makeAsyncChannelUnidirectionalBridge(
        msgBody.get(), this);

    const int id = ++m_lastId;
    bridge->start(
        [this, id](auto resultCode)
        {
            finishSendingBody(id, resultCode);
        });

    auto ctx = std::make_unique<detail::SendBodyContext>();
    ctx->msgBody = std::move(msgBody);
    ctx->bridge = std::move(bridge);
    ctx->handler = std::move(handler);
    m_sendBodyOperations.emplace(id, std::move(ctx));
}

void AsyncMessagePipeline::finishSendingBody(
    int id, SystemError::ErrorCode result)
{
    auto it = m_sendBodyOperations.find(id);
    NX_CRITICAL(it != m_sendBodyOperations.end());

    auto handler = std::move(it->second->handler);
    m_sendBodyOperations.erase(it);

    handler(result);
}

} // namespace nx::network::http
