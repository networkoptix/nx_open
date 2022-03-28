// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

namespace nx::network::server::detail {

/**
 * Helper class to gather statistics per server connection. Any *Connection class that provides the
 * detail::ConnectionStatistics& connectionStatistics() member function will enable statistics
 * gathering by the nx::network::server::StreamServerConnectionHolder class.
 */
class NX_NETWORK_API ConnectionStatistics
{
public:
    unsigned int messagesReceivedCount() const;

    /**
     * Install an event handler to be notified when a new message is received.
     * NOTE: To be used by the StreamSocketServer class only.
     */
    void setMessageReceivedHandler(nx::utils::MoveOnlyFunc<void()> handler);

    /**
     * Increments received messages counter and invokes messageReceivedHandler, if one is set
     * NOTE: Should be called by the *Connection class that own the ConnectionStatistics object
     * when a message is parsed.
     */
    void messageReceived();

private:
    unsigned int m_messagesReceivedCount{0};
    nx::utils::MoveOnlyFunc<void()> m_messageReceivedHandler;
};

} // namespace nx::network::server::detail
