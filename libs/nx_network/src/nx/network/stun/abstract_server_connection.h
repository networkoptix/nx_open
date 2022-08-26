// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <optional>

#include <nx/network/abstract_socket.h>
#include <nx/network/socket_common.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/stree/attribute_dictionary.h>
#include <nx/utils/system_error.h>
#include <nx/utils/string.h>

#include "message.h"

namespace nx {
namespace network {
namespace stun {

class AbstractServerConnection
{
public:
    virtual ~AbstractServerConnection() = default;

    /**
     * @param handler Triggered to report send result. Can be NULL.
     */
    virtual void sendMessage(
        nx::network::stun::Message message,
        std::function<void(SystemError::ErrorCode)> handler = nullptr) = 0;

    virtual nx::network::TransportProtocol transportProtocol() const = 0;
    virtual SocketAddress getSourceAddress() const = 0;

    /**
     * NOTE: AbstractServerConnection::sendMessage does nothing after handler has been invoked.
     */
    virtual void addOnConnectionCloseHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;

    virtual AbstractCommunicatingSocket* socket() = 0;
    virtual void close() = 0;
    virtual void setInactivityTimeout(std::optional<std::chrono::milliseconds> value) = 0;
};

} // namespace stun
} // namespace network
} // namespace nx
