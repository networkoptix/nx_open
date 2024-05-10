// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/network/aio/detail/async_channel_unidirectional_bridge.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/debug/object_instance_counter.h>

#include "abstract_msg_body_source.h"
#include "http_parser.h"
#include "http_serializer.h"

namespace nx::network::aio::detail { class AsyncChannelUnidirectionalBridge; }

namespace nx::network::http {

namespace detail {

struct NX_NETWORK_API SendBodyContext
{
    std::unique_ptr<AbstractMsgBodySourceWithCache> msgBody;
    std::unique_ptr<aio::detail::AsyncChannelUnidirectionalBridge> bridge;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler;

private:
    nx::network::debug::ObjectInstanceCounter<SendBodyContext> m_instanceCounter;
};

} // namespace detail

/**
 * Class for sending/receiving HTTP messages.
 * Extends nx::network::server::StreamProtocolConnection by adding support for sending
 * HTTP body provided as AbstractMsgBodySourceWithCache instance.
 */
class NX_NETWORK_API AsyncMessagePipeline:
    public nx::network::server::StreamProtocolConnection<
        nx::network::http::Message,
        nx::network::http::MessageParser,
        nx::network::http::MessageSerializer>
{
    using base_type =
        nx::network::server::StreamProtocolConnection<
            nx::network::http::Message,
            nx::network::http::MessageParser,
            nx::network::http::MessageSerializer>;

public:
    using base_type::base_type;
    virtual ~AsyncMessagePipeline();

    using base_type::sendMessage;

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    void sendMessage(
        Message msg,
        std::unique_ptr<AbstractMsgBodySourceWithCache> msgBody,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    int m_lastId = 0;
    std::map<int, std::unique_ptr<detail::SendBodyContext>> m_sendBodyOperations;
    nx::network::debug::ObjectInstanceCounter<AsyncMessagePipeline> m_instanceCounter;

    void sendBodyAsync(
        std::unique_ptr<AbstractMsgBodySourceWithCache> msgBody,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler);

    void finishSendingBody(int id, SystemError::ErrorCode result);
};

} // namespace nx::network::http
