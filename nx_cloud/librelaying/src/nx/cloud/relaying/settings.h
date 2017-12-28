#pragma once

#include <chrono>

#include <nx/network/abstract_socket.h>
#include <nx/utils/settings.h>

class QnSettings;

namespace nx {
namespace cloud {
namespace relaying {

struct NX_RELAYING_API Settings
{
    int recommendedPreemptiveConnectionCount;
    int maxPreemptiveConnectionCount;
    std::chrono::milliseconds disconnectedPeerTimeout;
    std::chrono::milliseconds takeIdleConnectionTimeout;
    std::chrono::milliseconds internalTimerPeriod;
    network::KeepAliveOptions tcpKeepAlive;

    Settings();

    void load(const QnSettings& settings);
};

} // namespace relaying
} // namespace cloud
} // namespace nx
