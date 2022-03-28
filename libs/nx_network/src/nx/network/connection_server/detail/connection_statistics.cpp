// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_statistics.h"

namespace nx::network::server::detail {

void ConnectionStatistics::setMessageReceivedHandler(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_messageReceivedHandler = std::move(handler);
}

unsigned int ConnectionStatistics::messagesReceivedCount() const
{
    return m_messagesReceivedCount;
}

void ConnectionStatistics::messageReceived()
{
    ++m_messagesReceivedCount;
    if (m_messageReceivedHandler)
        m_messageReceivedHandler();
}

} // namespace nx::network::server::detail
